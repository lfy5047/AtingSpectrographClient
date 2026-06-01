#include "TifRenderWorker.h"

#include "tiffio.h"

#include <QByteArray>
#include <QRgb>

#include <algorithm>
#include <cstring>
#include <limits>

TifRenderWorker::TifRenderWorker(QObject* parent)
    : QObject(parent)
{
}

void TifRenderWorker::requestCancel(quint64 newestRequestId)
{
    newestRequestId_.store(newestRequestId);
    cancelRequested_.store(true);
}

void TifRenderWorker::render(TifRenderRequest request)
{
    if (request.requestId != newestRequestId_.load()) {
        emit canceled(request.requestId);
        return;
    }
    cancelRequested_.store(false);

    QImage image;
    QString err;
    QString info;
    const bool ok = renderRequest(request, &image, &info, &err);
    if (request.requestId != newestRequestId_.load() || cancelRequested_.load()) {
        emit canceled(request.requestId);
        return;
    }
    if (!ok) {
        emit failed(request.requestId, request.globalIndex, err);
        return;
    }
    emit ready(request.requestId, request.globalIndex, image, info, imageStatsFromImage(image));
}

bool TifRenderWorker::shouldCancel(const TifRenderRequest& request) const
{
    return cancelRequested_.load() || request.requestId != newestRequestId_.load();
}

bool TifRenderWorker::normalizeRange(int pageCount, int begin, int end, int* outBegin, int* outEnd, QString* err) const
{
    if (begin < 0 || begin >= pageCount || end <= begin || end > pageCount) {
        if (err) *err = QString::fromUtf8("波段范围非法，要求 0 <= begin < end <= page_count");
        return false;
    }
    if (outBegin) *outBegin = begin;
    if (outEnd) *outEnd = end;
    return true;
}

bool TifRenderWorker::readPage(TIFF* tif, const TifRenderRequest& request, int pageIndex,
                               QVector<quint16>* out, QString* err) const
{
    if (shouldCancel(request)) return false;
    if (!TIFFSetDirectory(tif, static_cast<tdir_t>(pageIndex))) {
        if (err) *err = QString::fromUtf8("无法定位 tif page %1").arg(pageIndex + 1);
        return false;
    }
    const tsize_t scanline = TIFFScanlineSize(tif);
    if (scanline < request.width * static_cast<tsize_t>(sizeof(quint16))) {
        if (err) *err = QString::fromUtf8("tif scanline 尺寸异常");
        return false;
    }
    const int pixels = request.width * request.height;
    out->resize(pixels);
    QByteArray row(static_cast<int>(scanline), '\0');
    for (int y = 0; y < request.height; ++y) {
        if (shouldCancel(request)) return false;
        if (TIFFReadScanline(tif, row.data(), y, 0) < 0) {
            if (err) *err = QString::fromUtf8("读取 tif scanline 失败");
            return false;
        }
        std::memcpy(out->data() + y * request.width, row.constData(), request.width * sizeof(quint16));
    }
    return true;
}

bool TifRenderWorker::renderGrayRange(TIFF* tif, const TifRenderRequest& request, int begin, int end,
                                      QImage* image, QString* err)
{
    int b = 0;
    int e = 0;
    if (!normalizeRange(request.pageCount, begin, end, &b, &e, err)) return false;
    const int pixels = request.width * request.height;
    QVector<quint64> acc(pixels, 0);
    QVector<quint16> page;
    for (int p = b; p < e; ++p) {
        emit progress(request.requestId, QString::fromUtf8("正在渲染 TIF...（波段 %1/%2）").arg(p + 1).arg(request.pageCount));
        if (!readPage(tif, request, p, &page, err)) return false;
        for (int i = 0; i < pixels; ++i) acc[i] += page[i];
    }

    const int count = e - b;
    int minV = std::numeric_limits<int>::max();
    int maxV = 0;
    QVector<int> values(pixels, 0);
    for (int i = 0; i < pixels; ++i) {
        values[i] = static_cast<int>(acc[i] / count);
        minV = std::min(minV, values[i]);
        maxV = std::max(maxV, values[i]);
    }
    QByteArray gray(pixels, '\0');
    if (maxV > minV) {
        const double scale = 255.0 / (maxV - minV);
        for (int i = 0; i < pixels; ++i) {
            gray[i] = static_cast<char>(qBound(0, static_cast<int>((values[i] - minV) * scale), 255));
        }
    }
    *image = QImage(reinterpret_cast<const uchar*>(gray.constData()),
                    request.width, request.height, request.width,
                    QImage::Format_Grayscale8).copy();
    return true;
}

bool TifRenderWorker::renderRgb(TIFF* tif, const TifRenderRequest& request,
                                int rb, int re, int gb, int ge, int bb, int be,
                                QImage* image, QString* err)
{
    if (!normalizeRange(request.pageCount, rb, re, &rb, &re, err) ||
        !normalizeRange(request.pageCount, gb, ge, &gb, &ge, err) ||
        !normalizeRange(request.pageCount, bb, be, &bb, &be, err)) {
        return false;
    }

    const int pixels = request.width * request.height;
    auto readRange = [&](int begin, int end, QVector<int>* out) -> bool {
        QVector<quint64> acc(pixels, 0);
        QVector<quint16> page;
        for (int p = begin; p < end; ++p) {
            emit progress(request.requestId, QString::fromUtf8("正在渲染 TIF...（波段 %1/%2）").arg(p + 1).arg(request.pageCount));
            if (!readPage(tif, request, p, &page, err)) return false;
            for (int i = 0; i < pixels; ++i) acc[i] += page[i];
        }
        out->resize(pixels);
        const int count = end - begin;
        for (int i = 0; i < pixels; ++i) (*out)[i] = static_cast<int>(acc[i] / count);
        return true;
    };

    QVector<int> rv;
    QVector<int> gv;
    QVector<int> bv;
    if (!readRange(rb, re, &rv) || !readRange(gb, ge, &gv) || !readRange(bb, be, &bv)) {
        return false;
    }
    auto minmax = [](const QVector<int>& v, int* mn, int* mx) {
        *mn = std::numeric_limits<int>::max();
        *mx = 0;
        for (int x : v) {
            *mn = std::min(*mn, x);
            *mx = std::max(*mx, x);
        }
    };
    int rMin = 0;
    int rMax = 0;
    int gMin = 0;
    int gMax = 0;
    int bMin = 0;
    int bMax = 0;
    minmax(rv, &rMin, &rMax);
    minmax(gv, &gMin, &gMax);
    minmax(bv, &bMin, &bMax);
    QByteArray rgb(pixels * 3, '\0');
    for (int i = 0; i < pixels; ++i) {
        const int out = i * 3;
        rgb[out] = static_cast<char>(rMax > rMin ? qBound(0, static_cast<int>((rv[i] - rMin) * 255.0 / (rMax - rMin)), 255) : 0);
        rgb[out + 1] = static_cast<char>(gMax > gMin ? qBound(0, static_cast<int>((gv[i] - gMin) * 255.0 / (gMax - gMin)), 255) : 0);
        rgb[out + 2] = static_cast<char>(bMax > bMin ? qBound(0, static_cast<int>((bv[i] - bMin) * 255.0 / (bMax - bMin)), 255) : 0);
    }
    *image = QImage(reinterpret_cast<const uchar*>(rgb.constData()),
                    request.width, request.height, request.width * 3,
                    QImage::Format_RGB888).copy();
    return true;
}

bool TifRenderWorker::renderRequest(const TifRenderRequest& request, QImage* image, QString* info, QString* err)
{
    TIFF* tif = TIFFOpen(request.path.toLocal8Bit().constData(), "r");
    if (!tif) {
        if (err) *err = QString::fromUtf8("无法打开 tif: %1").arg(request.path);
        return false;
    }

    bool ok = false;
    if (request.mode == 0 || request.mode == 1) {
        ok = renderGrayRange(tif, request, request.a, request.b, image, err);
    } else {
        ok = renderRgb(tif, request, request.a, request.b, request.c, request.d, request.e, request.f, image, err);
    }
    TIFFClose(tif);
    if (!ok) return false;
    if (info) {
        *info = QString("tif %1 pages=%2 size=%3x%4")
            .arg(request.recordId)
            .arg(request.pageCount)
            .arg(request.width)
            .arg(request.height);
    }
    return true;
}

ChannelImageStats TifRenderWorker::imageStatsFromImage(const QImage& image)
{
    ChannelImageStats stats;
    if (image.isNull()) return stats;
    quint64 sum = 0;
    quint16 minV = std::numeric_limits<quint16>::max();
    quint16 maxV = 0;
    const QImage img = image.convertToFormat(QImage::Format_ARGB32);
    const int pixels = img.width() * img.height();
    if (pixels <= 0) return stats;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = row[x];
            const quint16 v = static_cast<quint16>((qRed(px) + qGreen(px) + qBlue(px)) / 3);
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            sum += v;
        }
    }
    stats.valid = true;
    stats.min = minV;
    stats.max = maxV;
    stats.avg = static_cast<double>(sum) / pixels;
    return stats;
}

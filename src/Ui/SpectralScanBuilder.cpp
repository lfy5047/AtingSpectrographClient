#include "SpectralScanBuilder.h"

#include <algorithm>

#include "Protocol.h"

namespace {
int clampBand(int v, int bands)
{
    if (bands <= 0) return 0;
    if (v < 0) return 0;
    if (v >= bands) return bands - 1;
    return v;
}
}

void SpectralScanBuilder::reset()
{
    columns_.clear();
    lastStreamFrameId_ = 0;
    streamFrameIdStep_ = 0;
    bands_ = 0;
    height_ = 0;
    pixfmt_ = 0;
    active_ = false;
    tailSeen_ = false;
    gapFillColumns_ = 0;
}

bool SpectralScanBuilder::hasCompatibleGeometry(int bands, int height, int pixfmt) const
{
    return bands_ == bands && height_ == height && pixfmt_ == pixfmt;
}

bool SpectralScanBuilder::appendColumn(const QByteArray& data)
{
    const int bpp = (pixfmt_ == cli::proto::Mono8) ? 1 : 2;
    if (bpp <= 0 || bands_ <= 0 || height_ <= 0) return false;
    const int expectedBytes = bands_ * height_ * bpp;
    if (data.size() < expectedBytes) return false;

    if (data.size() == expectedBytes) {
        columns_.append(data);
    } else {
        columns_.append(data.left(expectedBytes));
    }
    return true;
}

void SpectralScanBuilder::feedFrame(quint8 frameType, quint64 streamFrameId, int bands, int height, int pixfmt, const QByteArray& data)
{
    if (bands <= 0 || height <= 0) return;
    if (pixfmt != cli::proto::Mono8 && pixfmt != cli::proto::Mono16) return;

    if (frameType == cli::proto::HeaderFrame) {
        reset();
        bands_ = bands;
        height_ = height;
        pixfmt_ = pixfmt;
        active_ = true;
        tailSeen_ = false;
        if (appendColumn(data)) {
            lastStreamFrameId_ = streamFrameId;
        } else {
            reset();
        }
        return;
    }

    if (frameType != cli::proto::DataFrame && frameType != cli::proto::TailFrame) {
        return;
    }

    if (!active_) {
        return;
    }
    if (!hasCompatibleGeometry(bands, height, pixfmt)) {
        reset();
        return;
    }
    if (streamFrameId <= lastStreamFrameId_) {
        return;
    }

    const quint64 delta = streamFrameId - lastStreamFrameId_;
    if (streamFrameIdStep_ == 0) {
        streamFrameIdStep_ = delta;
    }

    const quint64 missingColumns = (streamFrameIdStep_ > 0 && delta > streamFrameIdStep_)
                                       ? (delta / streamFrameIdStep_) - 1
                                       : 0;
    if (missingColumns > 0 && !columns_.isEmpty()) {
        const QByteArray prev = columns_.last();
        for (quint64 i = 0; i < missingColumns; ++i) {
            columns_.append(prev);
        }
        gapFillColumns_ += missingColumns;
    }

    if (!appendColumn(data)) {
        reset();
        return;
    }

    lastStreamFrameId_ = streamFrameId;
    if (frameType == cli::proto::TailFrame) {
        active_ = false;
        tailSeen_ = true;
    }
}

bool SpectralScanBuilder::readSample(const QByteArray& col, int pixelIndex, int& sample) const
{
    if (pixfmt_ == cli::proto::Mono8) {
        if (pixelIndex < 0 || pixelIndex >= col.size()) return false;
        sample = static_cast<unsigned char>(col[pixelIndex]);
        return true;
    }

    const int byteIndex = pixelIndex * 2;
    if (byteIndex < 0 || byteIndex + 1 >= col.size()) return false;
    const unsigned char lo = static_cast<unsigned char>(col[byteIndex]);
    const unsigned char hi = static_cast<unsigned char>(col[byteIndex + 1]);
    sample = static_cast<int>(lo | (hi << 8));
    return true;
}

QImage SpectralScanBuilder::render(const SpectralRenderOptions& opts) const
{
    if (columns_.isEmpty() || bands_ <= 0 || height_ <= 0) return QImage();

    const int w = columns_.size();
    const int h = height_;
    const int bands = bands_;
    const int pixels = w * h;

    if (opts.mode == SpectralRenderOptions::RgbComposite) {
        const int rb = clampBand(opts.rBand, bands);
        const int gb = clampBand(opts.gBand, bands);
        const int bb = clampBand(opts.bBand, bands);

        QVector<int> rgbValues(pixels * 3, 0);
        int rMin = 0x7FFFFFFF, rMax = 0;
        int gMin = 0x7FFFFFFF, gMax = 0;
        int bMin = 0x7FFFFFFF, bMax = 0;

        for (int x = 0; x < w; ++x) {
            const QByteArray& col = columns_[x];
            for (int y = 0; y < h; ++y) {
                const int base = y * bands;
                int rv = 0, gv = 0, bv = 0;
                if (!readSample(col, base + rb, rv)) continue;
                if (!readSample(col, base + gb, gv)) continue;
                if (!readSample(col, base + bb, bv)) continue;

                rMin = std::min(rMin, rv);
                rMax = std::max(rMax, rv);
                gMin = std::min(gMin, gv);
                gMax = std::max(gMax, gv);
                bMin = std::min(bMin, bv);
                bMax = std::max(bMax, bv);
                const int outIndex = (y * w + x) * 3;
                rgbValues[outIndex] = rv;
                rgbValues[outIndex + 1] = gv;
                rgbValues[outIndex + 2] = bv;
            }
        }

        QByteArray rgb(pixels * 3, '\0');
        const double rs = (rMax > rMin) ? (255.0 / (rMax - rMin)) : 0.0;
        const double gs = (gMax > gMin) ? (255.0 / (gMax - gMin)) : 0.0;
        const double bs = (bMax > bMin) ? (255.0 / (bMax - bMin)) : 0.0;
        for (int i = 0; i < pixels; ++i) {
            const int idx = i * 3;
            const int rv = rgbValues[idx];
            const int gv = rgbValues[idx + 1];
            const int bv = rgbValues[idx + 2];
            rgb[idx] = static_cast<char>(rMax > rMin ? std::min(255, std::max(0, static_cast<int>((rv - rMin) * rs))) : 0);
            rgb[idx + 1] = static_cast<char>(gMax > gMin ? std::min(255, std::max(0, static_cast<int>((gv - gMin) * gs))) : 0);
            rgb[idx + 2] = static_cast<char>(bMax > bMin ? std::min(255, std::max(0, static_cast<int>((bv - bMin) * bs))) : 0);
        }

        return QImage(reinterpret_cast<const uchar*>(rgb.constData()), w, h, w * 3, QImage::Format_RGB888).copy();
    }

    int bandStart = clampBand(opts.rangeStart, bands);
    int bandEnd = clampBand(opts.rangeEnd, bands);
    if (opts.mode == SpectralRenderOptions::SingleBand) {
        bandStart = bandEnd = clampBand(opts.singleBand, bands);
    } else if (bandStart > bandEnd) {
        std::swap(bandStart, bandEnd);
    }

    QVector<int> grayValues(pixels, 0);
    int minV = 0x7FFFFFFF;
    int maxV = 0;
    const int count = std::max(1, bandEnd - bandStart + 1);

    for (int x = 0; x < w; ++x) {
        const QByteArray& col = columns_[x];
        for (int y = 0; y < h; ++y) {
            const int base = y * bands;
            int acc = 0;
            for (int b = bandStart; b <= bandEnd; ++b) {
                int sample = 0;
                if (!readSample(col, base + b, sample)) continue;
                acc += sample;
            }
            const int v = acc / count;
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            grayValues[y * w + x] = v;
        }
    }

    QByteArray gray(pixels, '\0');
    if (maxV > minV) {
        const double s = 255.0 / (maxV - minV);
        for (int i = 0; i < grayValues.size(); ++i) {
            const int v = grayValues[i];
            gray[i] = static_cast<char>(std::min(255, std::max(0, static_cast<int>((v - minV) * s))));
        }
    } else {
        gray.fill('\0');
    }

    return QImage(reinterpret_cast<const uchar*>(gray.constData()), w, h, w, QImage::Format_Grayscale8).copy();
}

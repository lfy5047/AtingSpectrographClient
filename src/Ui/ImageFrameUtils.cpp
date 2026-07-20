#include "ImageFrameUtils.h"

#include "Protocol.h"

#include <QVector>

bool computeMono16Stats(const QByteArray& data, int width, int height, Mono16Stats* out)
{
    if (!out) return false;
    const int pixels = width * height;
    if (width <= 0 || height <= 0 || data.size() < pixels * 2) return false;

    const auto* src = reinterpret_cast<const quint16*>(data.constData());
    quint16 mn = 0xFFFF;
    quint16 mx = 0;
    quint64 sum = 0;
    for (int i = 0; i < pixels; ++i) {
        const quint16 v = src[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
    }

    out->min = mn;
    out->max = mx;
    out->sum = sum;
    return true;
}

QByteArray stretchCropMono16Horizontal(const QByteArray& data, int width, int height,
                                       int stretchWidth, int cropLeftColumns)
{
    const int pixelCount = width * height;
    const int requiredBytes = pixelCount * static_cast<int>(sizeof(quint16));
    if (width <= 0 || height <= 0 || data.size() < requiredBytes) return QByteArray();
    if (stretchWidth <= width) return data;

    const int extraColumns = stretchWidth - width;
    const int cropLeft = qBound(0, cropLeftColumns, extraColumns);
    const qint64 denominator = stretchWidth - 1;
    const auto* src = reinterpret_cast<const quint16*>(data.constData());
    QByteArray result(requiredBytes, '\0');
    auto* dst = reinterpret_cast<quint16*>(result.data());

    QVector<int> leftXs(width);
    QVector<qint64> remainders(width);
    for (int outputX = 0; outputX < width; ++outputX) {
        const int stretchedX = cropLeft + outputX;
        const qint64 numerator = static_cast<qint64>(stretchedX) * (width - 1);
        leftXs[outputX] = static_cast<int>(numerator / denominator);
        remainders[outputX] = numerator % denominator;
    }

    for (int y = 0; y < height; ++y) {
        const int rowOffset = y * width;
        for (int outputX = 0; outputX < width; ++outputX) {
            const int leftX = leftXs[outputX];
            const int rightX = qMin(width - 1, leftX + 1);
            const qint64 remainder = remainders[outputX];
            const qint64 leftWeight = denominator - remainder;
            const qint64 value = static_cast<qint64>(src[rowOffset + leftX]) * leftWeight
                + static_cast<qint64>(src[rowOffset + rightX]) * remainder;
            dst[rowOffset + outputX] = static_cast<quint16>((value + denominator / 2) / denominator);
        }
    }

    return result;
}

QImage makeMono16DisplayImage(int width, int height, const QByteArray& data,
                              quint16 displayMin, quint16 displayMax)
{
    const int pixels = width * height;
    if (width <= 0 || height <= 0 || data.size() < pixels * 2) return QImage();

    const auto* src = reinterpret_cast<const quint16*>(data.constData());
    QByteArray buf8(pixels, '\0');
    if (displayMax > displayMin) {
        const int range = static_cast<int>(displayMax) - static_cast<int>(displayMin);
        for (int i = 0; i < pixels; ++i) {
            const int bounded = qBound(static_cast<int>(displayMin),
                                       static_cast<int>(src[i]),
                                       static_cast<int>(displayMax));
            const int mapped = (bounded - static_cast<int>(displayMin)) * 255 / range;
            buf8[i] = static_cast<char>(static_cast<quint8>(mapped));
        }
    }

    return QImage(reinterpret_cast<const uchar*>(buf8.constData()),
                  width, height, width, QImage::Format_Grayscale8).copy();
}

QImage makeDisplayImage(int width, int height, int pixfmt, const QByteArray& data)
{
    using namespace cli::proto;

    if (width <= 0 || height <= 0) return QImage();

    if (pixfmt == Mono8) {
        if (data.size() < width * height) return QImage();
        return QImage(reinterpret_cast<const uchar*>(data.constData()),
                      width, height, width, QImage::Format_Grayscale8).copy();
    }

    if (pixfmt == Jpeg) {
        QImage image;
        if (!image.loadFromData(data, "JPG")) return QImage();
        if (image.width() != width || image.height() != height) return QImage();
        return image;
    }

    const int pixels = width * height;
    if (data.size() < pixels * 2) return QImage();

    const auto* src = reinterpret_cast<const quint16*>(data.constData());
    quint16 mn = 0xFFFF;
    quint16 mx = 0;
    for (int i = 0; i < pixels; ++i) {
        if (src[i] < mn) mn = src[i];
        if (src[i] > mx) mx = src[i];
    }

    return makeMono16DisplayImage(width, height, data, mn, mx);
}

ChannelImageStats makeChannelImageStats(int width, int height, int pixfmt, const QByteArray& data)
{
    using namespace cli::proto;

    ChannelImageStats stats;
    if (pixfmt != Mono16) return stats;

    Mono16Stats monoStats;
    if (!computeMono16Stats(data, width, height, &monoStats)) return stats;

    const int pixels = width * height;
    stats.valid = true;
    stats.min = monoStats.min;
    stats.max = monoStats.max;
    stats.avg = pixels > 0 ? static_cast<double>(monoStats.sum) / pixels : 0.0;
    return stats;
}

ComputeImageResult computeImageResult(int width, int height, int pixfmt, const QByteArray& data)
{
    ComputeImageResult result;
    result.display = makeDisplayImage(width, height, pixfmt, data);
    result.stats = makeChannelImageStats(width, height, pixfmt, data);
    return result;
}

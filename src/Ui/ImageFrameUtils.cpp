#include "ImageFrameUtils.h"

#include "Protocol.h"

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

QImage makeDisplayImage(int width, int height, int pixfmt, const QByteArray& data)
{
    using namespace cli::proto;

    if (width <= 0 || height <= 0) return QImage();

    if (pixfmt == Mono8) {
        if (data.size() < width * height) return QImage();
        return QImage(reinterpret_cast<const uchar*>(data.constData()),
                      width, height, width, QImage::Format_Grayscale8).copy();
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

    QByteArray buf8(pixels, '\0');
    if (mx > mn) {
        const double scale = 255.0 / (mx - mn);
        for (int i = 0; i < pixels; ++i) {
            buf8[i] = static_cast<char>(static_cast<quint8>((src[i] - mn) * scale));
        }
    }

    return QImage(reinterpret_cast<const uchar*>(buf8.constData()),
                  width, height, width, QImage::Format_Grayscale8).copy();
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

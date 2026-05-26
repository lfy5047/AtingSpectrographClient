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

void SpectralScanBuilder::resetActiveScan()
{
    columns_.clear();
    columnVersion_ = 0;
    lastStreamFrameId_ = 0;
    streamFrameIdStep_ = 0;
    bands_ = 0;
    height_ = 0;
    pixfmt_ = 0;
    active_ = false;
    tailSeen_ = false;
    gapFillColumns_ = 0;
}

void SpectralScanBuilder::commitActiveScan()
{
    completedColumns_.swap(columns_);
    columns_.clear();
    completedBands_ = bands_;
    completedHeight_ = height_;
    completedPixfmt_ = pixfmt_;
    completedTailSeen_ = true;
    completedGapFillColumns_ = gapFillColumns_;
    ++completedGeneration_;
    resetActiveScan();
}

void SpectralScanBuilder::reset()
{
    resetActiveScan();
    completedColumns_.clear();
    completedBands_ = 0;
    completedHeight_ = 0;
    completedPixfmt_ = 0;
    completedTailSeen_ = false;
    completedGapFillColumns_ = 0;
    ++completedGeneration_;
    rangeAverageCache_ = RangeAverageCache();
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
    ++columnVersion_;
    return true;
}

bool SpectralScanBuilder::feedFrame(quint8 frameType, quint64 streamFrameId, int bands, int height, int pixfmt, const QByteArray& data)
{
    if (bands <= 0 || height <= 0) return false;
    if (pixfmt != cli::proto::Mono8 && pixfmt != cli::proto::Mono16) return false;

    if (frameType == cli::proto::HeaderFrame) {
        resetActiveScan();
        bands_ = bands;
        height_ = height;
        pixfmt_ = pixfmt;
        active_ = true;
        tailSeen_ = false;
        if (appendColumn(data)) {
            lastStreamFrameId_ = streamFrameId;
        } else {
            resetActiveScan();
        }
        return false;
    }

    if (frameType != cli::proto::DataFrame && frameType != cli::proto::TailFrame) {
        return false;
    }

    if (!active_) {
        return false;
    }
    if (!hasCompatibleGeometry(bands, height, pixfmt)) {
        resetActiveScan();
        return false;
    }
    if (streamFrameId <= lastStreamFrameId_) {
        return false;
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
            ++columnVersion_;
        }
        gapFillColumns_ += missingColumns;
    }

    if (!appendColumn(data)) {
        resetActiveScan();
        return false;
    }

    lastStreamFrameId_ = streamFrameId;
    if (frameType == cli::proto::TailFrame) {
        commitActiveScan();
        return true;
    }
    return false;
}

bool SpectralScanBuilder::readSample(const QByteArray& col, int pixelIndex, int pixfmt, int& sample) const
{
    if (pixfmt == cli::proto::Mono8) {
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
    if (completedColumns_.isEmpty() || completedBands_ <= 0 || completedHeight_ <= 0) return QImage();

    const int w = completedColumns_.size();
    const int h = completedHeight_;
    const int bands = completedBands_;
    const int pixfmt = completedPixfmt_;
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
            const QByteArray& col = completedColumns_[x];
            for (int y = 0; y < h; ++y) {
                const int base = y * bands;
                int rv = 0, gv = 0, bv = 0;
                if (!readSample(col, base + rb, pixfmt, rv)) continue;
                if (!readSample(col, base + gb, pixfmt, gv)) continue;
                if (!readSample(col, base + bb, pixfmt, bv)) continue;

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

    const int count = std::max(1, bandEnd - bandStart + 1);
    const bool cacheCompatible =
        rangeAverageCache_.valid &&
        rangeAverageCache_.bandStart == bandStart &&
        rangeAverageCache_.bandEnd == bandEnd &&
        rangeAverageCache_.bands == bands &&
        rangeAverageCache_.height == h &&
        rangeAverageCache_.pixfmt == pixfmt;

    const bool versionMismatch = rangeAverageCache_.valid && (rangeAverageCache_.generation != completedGeneration_);
    if (!cacheCompatible || versionMismatch) {
        rangeAverageCache_ = RangeAverageCache();
        rangeAverageCache_.valid = true;
        rangeAverageCache_.bandStart = bandStart;
        rangeAverageCache_.bandEnd = bandEnd;
        rangeAverageCache_.bands = bands;
        rangeAverageCache_.height = h;
        rangeAverageCache_.pixfmt = pixfmt;
    }

    if (rangeAverageCache_.cachedColumns > w) {
        rangeAverageCache_.cachedColumns = 0;
        rangeAverageCache_.columnValues.clear();
    }

    const int oldColumns = rangeAverageCache_.cachedColumns;
    if (oldColumns < w) {
        rangeAverageCache_.columnValues.resize(w * h);
        if (pixfmt == cli::proto::Mono8) {
            for (int x = oldColumns; x < w; ++x) {
                const QByteArray& col = completedColumns_[x];
                const uchar* src = reinterpret_cast<const uchar*>(col.constData());
                for (int y = 0; y < h; ++y) {
                    const int base = y * bands + bandStart;
                    qint64 acc = 0;
                    for (int b = 0; b < count; ++b) {
                        acc += src[base + b];
                    }
                    rangeAverageCache_.columnValues[x * h + y] = static_cast<int>(acc / count);
                }
            }
        } else {
            for (int x = oldColumns; x < w; ++x) {
                const QByteArray& col = completedColumns_[x];
                // Windows x86/x64 target accepts unaligned reads for uint16 payload.
                const uint16_t* src = reinterpret_cast<const uint16_t*>(col.constData());
                for (int y = 0; y < h; ++y) {
                    const int base = y * bands + bandStart;
                    qint64 acc = 0;
                    for (int b = 0; b < count; ++b) {
                        acc += src[base + b];
                    }
                    rangeAverageCache_.columnValues[x * h + y] = static_cast<int>(acc / count);
                }
            }
        }
        rangeAverageCache_.cachedColumns = w;
    }
    rangeAverageCache_.generation = completedGeneration_;

    int minV = 0x7FFFFFFF;
    int maxV = 0;
    for (int x = 0; x < w; ++x) {
        const int columnBase = x * h;
        for (int y = 0; y < h; ++y) {
            const int v = rangeAverageCache_.columnValues[columnBase + y];
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
        }
    }

    QByteArray gray(pixels, '\0');
    if (maxV > minV) {
        const double s = 255.0 / (maxV - minV);
        for (int y = 0; y < h; ++y) {
            const int rowBase = y * w;
            for (int x = 0; x < w; ++x) {
                const int v = rangeAverageCache_.columnValues[x * h + y];
                gray[rowBase + x] = static_cast<char>(std::min(255, std::max(0, static_cast<int>((v - minV) * s))));
            }
        }
    } else {
        gray.fill('\0');
    }

    return QImage(reinterpret_cast<const uchar*>(gray.constData()), w, h, w, QImage::Format_Grayscale8).copy();
}

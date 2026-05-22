#pragma once

#include <QByteArray>
#include <QImage>
#include <QVector>
#include <QtGlobal>

struct SpectralRenderOptions {
    enum Mode {
        SingleBand = 0,
        RangeAverage = 1,
        RgbComposite = 2,
    };

    Mode mode = SingleBand;
    int singleBand = 0;
    int rangeStart = 0;
    int rangeEnd = 0;
    int rBand = 0;
    int gBand = 0;
    int bBand = 0;
};

class SpectralScanBuilder {
public:
    void reset();
    void feedFrame(quint8 frameType, quint64 streamFrameId, int bands, int height, int pixfmt, const QByteArray& data);

    bool hasData() const { return !columns_.isEmpty(); }
    bool hasActiveScan() const { return active_; }
    bool tailSeen() const { return tailSeen_; }
    int bands() const { return bands_; }
    int height() const { return height_; }
    int scanWidth() const { return columns_.size(); }
    quint64 gapFillColumns() const { return gapFillColumns_; }

    QImage render(const SpectralRenderOptions& opts) const;

private:
    struct RangeAverageCache {
        bool valid = false;
        int bandStart = 0;
        int bandEnd = 0;
        int bands = 0;
        int height = 0;
        int pixfmt = 0;
        quint64 columnVersion = 0;
        int cachedColumns = 0;
        QVector<int> columnValues;
    };

    bool hasCompatibleGeometry(int bands, int height, int pixfmt) const;
    bool appendColumn(const QByteArray& data);
    bool readSample(const QByteArray& col, int pixelIndex, int& sample) const;

    QVector<QByteArray> columns_;
    quint64 columnVersion_ = 0;
    quint64 lastStreamFrameId_ = 0;
    quint64 streamFrameIdStep_ = 0;
    int bands_ = 0;
    int height_ = 0;
    int pixfmt_ = 0;
    bool active_ = false;
    bool tailSeen_ = false;
    quint64 gapFillColumns_ = 0;
    mutable RangeAverageCache rangeAverageCache_;
};

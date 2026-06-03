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
    bool feedFrame(quint8 frameType, quint64 streamFrameId, int bands, int height, int pixfmt,
                   const QByteArray& data, bool hasScanDirection, bool reverseScan);

    bool hasData() const { return hasRenderableData(); }
    bool hasRenderableData() const { return !completedColumns_.isEmpty() || !columns_.isEmpty(); }
    bool hasActiveScan() const { return active_; }
    bool tailSeen() const { return active_ ? tailSeen_ : completedTailSeen_; }
    int bands() const { return completedBands_ > 0 ? completedBands_ : bands_; }
    int height() const { return completedHeight_ > 0 ? completedHeight_ : height_; }
    int scanWidth() const { return active_ ? columns_.size() : completedColumns_.size(); }
    quint64 gapFillColumns() const { return active_ ? gapFillColumns_ : completedGapFillColumns_; }

    QImage render(const SpectralRenderOptions& opts) const;

private:
    struct RangeAverageCache {
        bool valid = false;
        int bandStart = 0;
        int bandEnd = 0;
        int bands = 0;
        int height = 0;
        int pixfmt = 0;
        quint64 generation = 0;
        int cachedColumns = 0;
        QVector<int> columnValues;
    };

    void resetActiveScan();
    void commitActiveScan(bool tailSeen);
    void logIssueRateLimited(const char* reason, quint8 frameType, quint64 streamFrameId,
                             int bands, int height, int pixfmt, qsizetype dataBytes) const;
    bool hasCompatibleGeometry(int bands, int height, int pixfmt) const;
    bool updateScanDirection(bool hasScanDirection, bool reverseScan);
    bool appendColumn(const QByteArray& data);
    bool readSample(const QByteArray& col, int pixelIndex, int pixfmt, int& sample) const;

    QVector<QByteArray> columns_;
    QVector<QByteArray> completedColumns_;
    quint64 columnVersion_ = 0;
    quint64 completedGeneration_ = 0;
    quint64 lastStreamFrameId_ = 0;
    quint64 streamFrameIdStep_ = 0;
    int bands_ = 0;
    int height_ = 0;
    int pixfmt_ = 0;
    int completedBands_ = 0;
    int completedHeight_ = 0;
    int completedPixfmt_ = 0;
    bool active_ = false;
    bool tailSeen_ = false;
    bool hasScanDirection_ = false;
    bool reverseScan_ = false;
    quint64 gapFillColumns_ = 0;
    bool completedTailSeen_ = false;
    bool completedHasScanDirection_ = false;
    bool completedReverseScan_ = false;
    quint64 completedGapFillColumns_ = 0;
    mutable qint64 lastIssueLogMs_ = 0;
    mutable RangeAverageCache rangeAverageCache_;
};

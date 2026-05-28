#pragma once

#include <QImage>
#include <QtGlobal>
#include <array>

#include "SpectralScanBuilder.h"
#include "panels/SpectralPanel.h"

struct RecordedFrame;
struct StreamFrame;

enum class SpectralSource {
    Live = 0,
    Playback = 1,
};

struct SpectralFrameResult {
    bool committed = false;
    bool progressChanged = false;
};

struct SpectralStats {
    int scanWidth = 0;
    int height = 0;
    int bands = 0;
    bool tailSeen = false;
    bool active = false;
    quint64 gapFillColumns = 0;
};

struct SpectralProgress {
    bool active = false;
    int percent = 0;
};

class SpectralScanController {
public:
    void setPlaybackActive(bool active) { playbackActive_ = active; }
    bool playbackActive() const { return playbackActive_; }

    SpectralSource effectiveSource(SpectralSourceMode mode) const;
    void setSource(SpectralSource source) { spectralSource_ = source; }
    SpectralSource source() const { return spectralSource_; }

    SpectralFrameResult feedLiveFrame(const StreamFrame& frame, qint64 nowMs);
    SpectralFrameResult feedPlaybackFrame(const RecordedFrame& frame, qint64 nowMs);

    SpectralStats stats(SpectralSource source, int channel) const;
    QImage render(SpectralSource source, int channel, const SpectralRenderOptions& options) const;
    SpectralProgress progress(SpectralSource source, int channel, qint64 nowMs) const;
    void clearPlayback();

private:
    struct SpectralProgressState {
        bool active = false;
        qint64 startedMs = -1;
    };

    SpectralScanBuilder* builderFor(SpectralSource source, int channel);
    const SpectralScanBuilder* builderFor(SpectralSource source, int channel) const;
    SpectralFrameResult feedFrame(SpectralSource source, int channel, quint8 frameType,
                                  quint64 streamFrameId, int width, int height,
                                  int pixfmt, const QByteArray& data, qint64 nowMs);
    bool startProgress(SpectralSource source, int channel, qint64 nowMs);
    bool stopProgress(SpectralSource source, int channel);
    void clearProgress(SpectralSource source);
    int progressIndex(SpectralSource source, int channel) const;

    SpectralScanBuilder liveRawSpectral_;
    SpectralScanBuilder liveSliceSpectral_;
    SpectralScanBuilder playbackRawSpectral_;
    SpectralScanBuilder playbackSliceSpectral_;
    SpectralSource spectralSource_ = SpectralSource::Live;
    bool playbackActive_ = false;
    std::array<SpectralProgressState, 4> progressStates_ = {};
};

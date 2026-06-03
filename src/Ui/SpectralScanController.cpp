#include "SpectralScanController.h"

#include "Client/recording/RecordedFrame.h"
#include "Client/stream/StreamFrame.h"
#include "Protocol.h"

SpectralSource SpectralScanController::effectiveSource(SpectralSourceMode mode) const
{
    switch (mode) {
    case SpectralSourceMode::Live:
        return SpectralSource::Live;
    case SpectralSourceMode::Playback:
        return SpectralSource::Playback;
    case SpectralSourceMode::Auto:
    default:
        return playbackActive_ ? SpectralSource::Playback : SpectralSource::Live;
    }
}

SpectralFrameResult SpectralScanController::feedLiveFrame(const StreamFrame& frame, qint64 nowMs)
{
    return feedFrame(SpectralSource::Live, frame.channel, frame.frameType, frame.streamFrameId,
                     frame.width, frame.height, frame.pixfmt, frame.data,
                     frame.hasScanDirection, frame.reverseScan, nowMs);
}

SpectralFrameResult SpectralScanController::feedPlaybackFrame(const RecordedFrame& frame, qint64 nowMs)
{
    return feedFrame(SpectralSource::Playback, frame.channel, frame.frameType, frame.streamFrameId,
                     frame.width, frame.height, frame.pixfmt, frame.data, false, false, nowMs);
}

SpectralStats SpectralScanController::stats(SpectralSource source, int channel) const
{
    SpectralStats result;
    const SpectralScanBuilder* b = builderFor(source, channel);
    if (!b) return result;

    result.scanWidth = b->scanWidth();
    result.height = b->height();
    result.bands = b->bands();
    result.tailSeen = b->tailSeen();
    result.active = b->hasActiveScan();
    result.gapFillColumns = b->gapFillColumns();
    return result;
}

QImage SpectralScanController::render(SpectralSource source, int channel, const SpectralRenderOptions& options) const
{
    const SpectralScanBuilder* b = builderFor(source, channel);
    if (!b || !b->hasRenderableData()) return QImage();
    return b->render(options);
}

SpectralProgress SpectralScanController::progress(SpectralSource source, int channel, qint64 nowMs) const
{
    SpectralProgress result;
    const int idx = progressIndex(source, channel);
    if (idx < 0) return result;

    const auto& state = progressStates_[static_cast<std::size_t>(idx)];
    if (!state.active || state.startedMs < 0) return result;

    const qint64 elapsedMs = qMax<qint64>(0, nowMs - state.startedMs);
    result.active = true;
    result.percent = elapsedMs >= 10000 ? 100 : static_cast<int>((elapsedMs * 100) / 10000);
    return result;
}

void SpectralScanController::clearPlayback()
{
    playbackRawSpectral_.reset();
    playbackSliceSpectral_.reset();
    clearProgress(SpectralSource::Playback);
}

SpectralScanBuilder* SpectralScanController::builderFor(SpectralSource source, int channel)
{
    using namespace cli::proto;

    if (source == SpectralSource::Live) {
        if (channel == Raw16) return &liveRawSpectral_;
        if (channel == SliceStitch16) return &liveSliceSpectral_;
        return nullptr;
    }

    if (channel == Raw16) return &playbackRawSpectral_;
    if (channel == SliceStitch16) return &playbackSliceSpectral_;
    return nullptr;
}

const SpectralScanBuilder* SpectralScanController::builderFor(SpectralSource source, int channel) const
{
    return const_cast<SpectralScanController*>(this)->builderFor(source, channel);
}

SpectralFrameResult SpectralScanController::feedFrame(SpectralSource source, int channel, quint8 frameType,
                                                      quint64 streamFrameId, int width, int height,
                                                      int pixfmt, const QByteArray& data, bool hasScanDirection,
                                                      bool reverseScan, qint64 nowMs)
{
    using namespace cli::proto;

    SpectralFrameResult result;
    if (frameType != HeaderFrame && frameType != DataFrame && frameType != TailFrame) {
        return result;
    }

    SpectralScanBuilder* b = builderFor(source, channel);
    if (!b) return result;

    const bool wasActive = b->hasActiveScan();
    result.committed = b->feedFrame(frameType, streamFrameId, width, height, pixfmt, data,
                                    hasScanDirection, reverseScan);
    const bool hasActive = b->hasActiveScan();

    if (frameType == HeaderFrame) {
        if (hasActive) {
            result.progressChanged = startProgress(source, channel, nowMs);
        } else if (wasActive && !result.committed) {
            result.progressChanged = stopProgress(source, channel);
        }
    } else if (frameType == TailFrame) {
        if (result.committed) {
            result.progressChanged = stopProgress(source, channel);
        } else if (wasActive && !hasActive) {
            result.progressChanged = stopProgress(source, channel);
        }
    } else if (wasActive && !hasActive && !result.committed) {
        result.progressChanged = stopProgress(source, channel);
    }

    return result;
}

bool SpectralScanController::startProgress(SpectralSource source, int channel, qint64 nowMs)
{
    const int idx = progressIndex(source, channel);
    if (idx < 0) return false;

    auto& state = progressStates_[static_cast<std::size_t>(idx)];
    const bool changed = !state.active || state.startedMs != nowMs;
    state.active = true;
    state.startedMs = nowMs;
    return changed;
}

bool SpectralScanController::stopProgress(SpectralSource source, int channel)
{
    const int idx = progressIndex(source, channel);
    if (idx < 0) return false;

    auto& state = progressStates_[static_cast<std::size_t>(idx)];
    const bool changed = state.active || state.startedMs >= 0;
    state.active = false;
    state.startedMs = -1;
    return changed;
}

void SpectralScanController::clearProgress(SpectralSource source)
{
    const int sourceIndex = source == SpectralSource::Live ? 0 : 1;
    for (int i = 0; i < 2; ++i) {
        auto& state = progressStates_[static_cast<std::size_t>(sourceIndex * 2 + i)];
        state.active = false;
        state.startedMs = -1;
    }
}

int SpectralScanController::progressIndex(SpectralSource source, int channel) const
{
    using namespace cli::proto;

    const int sourceIndex = source == SpectralSource::Live ? 0 : 1;
    int channelIndex = -1;
    if (channel == Raw16) channelIndex = 0;
    else if (channel == SliceStitch16) channelIndex = 1;
    if (channelIndex < 0) return -1;
    return sourceIndex * 2 + channelIndex;
}

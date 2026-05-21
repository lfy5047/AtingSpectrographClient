#include "FramePlaybackController.h"
#include "FramePlaybackScanWorker.h"

#include <QFile>
#include <QThread>
#include <memory>

FramePlaybackController::FramePlaybackController(QObject* parent)
    : QObject(parent)
{
    tick_.setTimerType(Qt::PreciseTimer);
    connect(&tick_, &QTimer::timeout, this, &FramePlaybackController::onTick);

    scanWorker_ = new FramePlaybackScanWorker(this);
    scanThread_ = new QThread(this);
    scanWorker_->moveToThread(scanThread_);
    connect(scanThread_, &QThread::finished, scanWorker_, &QObject::deleteLater);
    connect(this, &FramePlaybackController::scanStarted, scanWorker_, &FramePlaybackScanWorker::scan, Qt::QueuedConnection);

    scanThread_->start();
}

FramePlaybackController::~FramePlaybackController()
{
    stop();
    closeFile();
    if (scanThread_) {
        scanThread_->quit();
        scanThread_->wait(2000);
    }
}

bool FramePlaybackController::isScanning() const { return scanning_; }
bool FramePlaybackController::isIndexReady() const { return indexReady_; }
bool FramePlaybackController::isPlaying() const { return playing_; }

QString FramePlaybackController::filePath() const { return path_; }
quint64 FramePlaybackController::frameCount() const { return static_cast<quint64>(index_.size()); }
quint64 FramePlaybackController::durationMs() const { return header_.durationMs; }
quint64 FramePlaybackController::damagedFrames() const { return damagedFrames_; }
bool FramePlaybackController::wasClosedOk() const { return (header_.flags & recording::FileFlagClosedOk) != 0; }

void FramePlaybackController::openFile(const QString& path)
{
    stop();
    closeFile();

    scanning_ = true;
    indexReady_ = false;
    path_ = path;
    index_.clear();
    damagedFrames_ = 0;
    damagedIndexSet_.clear();
    currentIndex_ = 0;
    playedCount_ = 0;
    emit scanStarted(path);
}

void FramePlaybackController::closeFile()
{
    stop();
    indexReady_ = false;
    index_.clear();
    header_ = recording::FileHeader();
    damagedFrames_ = 0;
    damagedIndexSet_.clear();
    currentIndex_ = 0;
    playedCount_ = 0;
    file_.reset();
}

void FramePlaybackController::setFps(int fps)
{
    fps_ = qBound(1, fps, 120);
    updateTimer();
}

void FramePlaybackController::setPlayFrameLimit(quint64 n)
{
    playFrameLimit_ = n;
}

void FramePlaybackController::setLoop(bool loop)
{
    loop_ = loop;
}

void FramePlaybackController::seekTo(quint64 index)
{
    if (!indexReady_) return;
    if (index >= static_cast<quint64>(index_.size())) return;
    currentIndex_ = index;
    playedCount_ = 0;
    windowStartIndex_ = index;
    windowFrameCount_ = 0;
    emit playbackPositionChanged(currentIndex_);
}

void FramePlaybackController::play()
{
    if (!indexReady_) return;
    if (index_.isEmpty()) {
        emit playbackError("empty recording");
        return;
    }
    if (!file_) {
        file_.reset(new QFile(path_));
        if (!file_->open(QIODevice::ReadOnly)) {
            emit playbackError(file_->errorString());
            file_.reset();
            return;
        }
    }
    if (playing_) return;

    if (currentIndex_ >= static_cast<quint64>(index_.size())) {
        currentIndex_ = 0;
    }

    playing_ = true;
    windowStartIndex_ = currentIndex_;
    const quint64 remaining = static_cast<quint64>(index_.size()) - windowStartIndex_;
    windowFrameCount_ = remaining;
    playedCount_ = 0;
    emit playbackStarted();
    updateTimer();
}

void FramePlaybackController::pause()
{
    if (!playing_) return;
    playing_ = false;
    tick_.stop();
    emit playbackPaused();
}

void FramePlaybackController::stop()
{
    if (!playing_) return;
    playing_ = false;
    tick_.stop();
    playedCount_ = 0;
    windowFrameCount_ = 0;
    emit playbackStopped();
}

void FramePlaybackController::updateTimer()
{
    if (!playing_) return;
    int intervalMs = qMax(1, static_cast<int>(1000.0 / fps_));
    tick_.start(intervalMs);
}

bool FramePlaybackController::loadFrameAt(quint64 idx, RecordedFrame& out, QString* err)
{
    if (!indexReady_ || !file_) return false;
    if (idx >= static_cast<quint64>(index_.size())) return false;

    const IndexEntry& e = index_[static_cast<int>(idx)];
    if (!file_->seek(static_cast<qint64>(e.frameHeaderOffset))) {
        if (err) *err = "seek failed";
        return false;
    }

    recording::FrameHeader fh;
    QString ferr;
    if (!recording::readFrameHeader(*file_, fh, &ferr)) {
        if (err) *err = ferr;
        return false;
    }

    QByteArray payload;
    payload.resize(static_cast<int>(fh.dataBytes));
    const qint64 r = file_->read(payload.data(), payload.size());
    if (r != payload.size()) {
        if (err) *err = "read payload failed";
        return false;
    }

    const quint32 crc = recording::crc32_ieee(payload);
    if (crc != fh.crc32) {
        if (!damagedIndexSet_.contains(idx)) {
            damagedIndexSet_.insert(idx);
            damagedFrames_ = static_cast<quint64>(damagedIndexSet_.size());
        }
        if (err) *err = "crc mismatch";
        return false;
    }

    out.channel = fh.channel;
    out.width = fh.width;
    out.height = fh.height;
    out.pixfmt = fh.pixfmt;
    out.frameIndex = fh.frameIndex;
    out.timestampMs = fh.timestampMs;
    out.data = payload;
    out.crc32 = crc;
    return true;
}

void FramePlaybackController::onTick()
{
    if (!playing_) return;
    if (!indexReady_) return;

    if (currentIndex_ >= static_cast<quint64>(index_.size())) {
        autoAdvanceOrStop();
        return;
    }

    RecordedFrame f;
    QString err;
    if (loadFrameAt(currentIndex_, f, &err)) {
        emit playbackFrameReady(f);
        emit playbackPositionChanged(currentIndex_);
        ++currentIndex_;
        ++playedCount_;
    } else {
        // CRC mismatch or read error: skip this frame and continue.
        emit playbackError(err);
        ++currentIndex_;
        ++playedCount_;
    }

    autoAdvanceOrStop();
}

void FramePlaybackController::autoAdvanceOrStop()
{
    const quint64 total = static_cast<quint64>(index_.size());
    const quint64 windowEnd = windowStartIndex_ + windowFrameCount_;
    const bool reachedFileEnd = currentIndex_ >= total;
    const bool reachedWindowEnd = currentIndex_ >= windowEnd;

    if (playFrameLimit_ > 0) {
        if (playedCount_ >= playFrameLimit_) {
            stop();
            return;
        }
        if (!reachedWindowEnd && !reachedFileEnd) return;
        if (!loop_) {
            stop();
            return;
        }
        currentIndex_ = windowStartIndex_;
        emit playbackPositionChanged(currentIndex_);
        return;
    }

    // playFrameLimit_ == 0: play to window end (file end) or loop
    if (!reachedFileEnd) return;
    if (!loop_) {
        stop();
        return;
    }
    currentIndex_ = windowStartIndex_;
    emit playbackPositionChanged(currentIndex_);
}


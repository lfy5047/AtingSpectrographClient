#include "FrameRecorderWriterWorker.h"

#include <QDateTime>
#include <QMutexLocker>

FrameRecorderWriterWorker::FrameRecorderWriterWorker(QObject* parent)
    : QObject(parent)
{}

bool FrameRecorderWriterWorker::isRecording() const
{
    QMutexLocker lk(&mu_);
    return recording_;
}

QString FrameRecorderWriterWorker::filePath() const
{
    QMutexLocker lk(&mu_);
    return path_;
}

quint64 FrameRecorderWriterWorker::framesWritten() const
{
    QMutexLocker lk(&mu_);
    return framesWritten_;
}

quint64 FrameRecorderWriterWorker::durationMs() const
{
    QMutexLocker lk(&mu_);
    return durationMs_;
}

quint64 FrameRecorderWriterWorker::bytesWritten() const
{
    QMutexLocker lk(&mu_);
    return bytesWritten_;
}

quint64 FrameRecorderWriterWorker::bytesQueued() const
{
    QMutexLocker lk(&mu_);
    return queuedBytes_;
}

quint64 FrameRecorderWriterWorker::droppedByBackpressure() const
{
    QMutexLocker lk(&mu_);
    return droppedByBackpressure_;
}

void FrameRecorderWriterWorker::start(const QString& path)
{
    bool started = false;
    QString error;

    QMutexLocker lk(&mu_);
    if (recording_) return;

    path_ = path;
    framesWritten_ = 0;
    durationMs_ = 0;
    bytesWritten_ = 0;
    queuedBytes_ = 0;
    droppedByBackpressure_ = 0;
    nextFrameIndex_ = 0;
    timer_.restart();
    stopRequested_ = false;
    lastError_.clear();

    file_.reset(new QFile(path_));
    if (!file_->open(QIODevice::ReadWrite)) {
        lastError_ = file_->errorString();
        error = lastError_;
        file_.reset();
    } else {
        recording::FileHeader fh;
        fh.createdUnixMs = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch());
        fh.frameCount = 0;
        fh.durationMs = 0;
        fh.flags = 0;
        if (!recording::writeFileHeader(*file_, fh)) {
            lastError_ = "write header failed";
            error = lastError_;
            file_->close();
            file_.reset();
        } else {
            header_ = fh;
            recording_ = true;
            started = true;
        }
    }

    lk.unlock();

    if (!error.isEmpty()) {
        emit recordingError(error);
        return;
    }
    if (!started) return;

    emit recordingStarted(path);
    emit statsChanged();
    cond_.wakeAll();
}

void FrameRecorderWriterWorker::stop()
{
    {
        QMutexLocker lk(&mu_);
        stopRequested_ = true;
        cond_.wakeAll();
    }
    // Drain is handled by writerLoop() when it sees stopRequested_.
}

void FrameRecorderWriterWorker::enqueueFrame(int channel, int width, int height, int pixfmt, const QByteArray& data)
{
    QByteArray copy = data;
    copy.detach();

    bool changed = false;
    QMutexLocker lk(&mu_);
    if (!recording_) return;

    static const quint64 kMaxQueueBytes = 256ull * 1024ull * 1024ull;
    const quint64 add = static_cast<quint64>(copy.size());
    if (queuedBytes_ + add > kMaxQueueBytes) {
        ++droppedByBackpressure_;
        changed = true;
        lk.unlock();
        if (changed) emit statsChanged();
        return;
    }

    QueuedItem it;
    it.channel = channel;
    it.width = width;
    it.height = height;
    it.pixfmt = pixfmt;
    it.data = copy;
    it.timestampMs = static_cast<quint64>(timer_.elapsed());
    it.frameIndex = nextFrameIndex_++;
    it.crc32 = recording::crc32_ieee(it.data);

    queuedBytes_ += add;
    queue_.enqueue(it);
    changed = true;
    lk.unlock();

    cond_.wakeAll();
    if (changed) emit statsChanged();
}

void FrameRecorderWriterWorker::writerLoop()
{
    for (;;) {
        QueuedItem item;
        {
            QMutexLocker lk(&mu_);
            while (queue_.isEmpty() && !stopRequested_) {
                cond_.wait(&mu_, 50);
            }

            if (stopRequested_) {
                break;
            }

            item = queue_.dequeue();
            queuedBytes_ -= static_cast<quint64>(item.data.size());
        }

        if (!writeOne(item)) {
            QMutexLocker lk(&mu_);
            stopRequested_ = true;
        }
    }

    drainAndFinalize();
}

bool FrameRecorderWriterWorker::writeOne(const QueuedItem& item)
{
    QString error;
    qint64 pos = 0;

    {
        QMutexLocker lk(&fileMu_);
        if (!file_) return false;

        recording::FrameHeader fh;
        fh.channel = static_cast<quint8>(item.channel);
        fh.width = static_cast<quint16>(item.width);
        fh.height = static_cast<quint16>(item.height);
        fh.pixfmt = static_cast<quint16>(item.pixfmt);
        fh.frameIndex = item.frameIndex;
        fh.timestampMs = item.timestampMs;
        fh.dataBytes = static_cast<quint32>(item.data.size());
        fh.crc32 = item.crc32;

        if (!recording::writeFrameHeader(*file_, fh)) {
            error = "write frame header failed";
        } else {
            const qint64 w = file_->write(item.data);
            if (w != item.data.size()) {
                error = file_->errorString().isEmpty() ? "write frame payload failed" : file_->errorString();
            } else {
                pos = file_->pos();
            }
        }
    }

    if (!error.isEmpty()) {
        {
            QMutexLocker lk(&mu_);
            lastError_ = error;
        }
        emit recordingError(error);
        return false;
    }

    {
        QMutexLocker lk2(&mu_);
        ++framesWritten_;
        durationMs_ = static_cast<quint64>(timer_.elapsed());
        bytesWritten_ = static_cast<quint64>(pos);
    }

    emit statsChanged();
    return true;
}

void FrameRecorderWriterWorker::drainAndFinalize()
{
    for (;;) {
        QueuedItem item;
        {
            QMutexLocker lk(&mu_);
            if (queue_.isEmpty()) break;
            item = queue_.dequeue();
            queuedBytes_ -= static_cast<quint64>(item.data.size());
        }
        if (!writeOne(item)) break;
    }

    finalize();
}

void FrameRecorderWriterWorker::finalize()
{
    QString path;
    {
        QMutexLocker lk(&mu_);
        if (!recording_) return;
        recording_ = false;
        path = path_;
    }

    bool ok = true;
    QString err;

    {
        QMutexLocker lk(&fileMu_);
        if (file_) {
            header_.frameCount = framesWritten();
            header_.durationMs = durationMs();
            header_.flags = header_.flags | recording::FileFlagClosedOk;

            if (!recording::rewriteFileHeader(*file_, header_)) {
                ok = false;
                err = "rewrite header failed";
            }
            file_->flush();
            file_->close();
            file_.reset();
        } else {
            ok = false;
            err = "file not open";
        }
    }

    emit recordingStopped(path, ok, err);
    emit statsChanged();
}

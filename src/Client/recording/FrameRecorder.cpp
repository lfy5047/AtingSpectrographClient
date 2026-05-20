#include "FrameRecorder.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include <memory>

#include "RecordingFileFormat.h"

class FrameRecorder::WriterWorker : public QObject {
    Q_OBJECT
public:
    explicit WriterWorker(QObject* parent = nullptr)
        : QObject(parent)
    {}

    bool isRecording() const
    {
        QMutexLocker lk(&mu_);
        return recording_;
    }

    QString filePath() const
    {
        QMutexLocker lk(&mu_);
        return path_;
    }

    quint64 framesWritten() const
    {
        QMutexLocker lk(&mu_);
        return framesWritten_;
    }

    quint64 durationMs() const
    {
        QMutexLocker lk(&mu_);
        return durationMs_;
    }

    quint64 bytesWritten() const
    {
        QMutexLocker lk(&mu_);
        return bytesWritten_;
    }

    quint64 bytesQueued() const
    {
        QMutexLocker lk(&mu_);
        return queuedBytes_;
    }

    quint64 droppedByBackpressure() const
    {
        QMutexLocker lk(&mu_);
        return droppedByBackpressure_;
    }

    void start(const QString& path)
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

    void stop()
    {
        {
            QMutexLocker lk(&mu_);
            stopRequested_ = true;
            cond_.wakeAll();
        }
        // Drain is now handled by writerLoop() when it sees stopRequested_.
        // Don't call flushLoop() here — it would race with writerLoop().
    }

    void enqueueFrame(int channel, int width, int height, int pixfmt, const QByteArray& data)
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

    void writerLoop()
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

signals:
    void recordingStarted(QString path);
    void recordingStopped(QString path, bool ok, QString err);
    void recordingError(QString err);
    void statsChanged();

private:
    struct QueuedItem {
        int channel = 0;
        int width = 0;
        int height = 0;
        int pixfmt = 0;
        quint64 frameIndex = 0;
        quint64 timestampMs = 0;
        quint32 crc32 = 0;
        QByteArray data;
    };

    bool writeOne(const QueuedItem& item)
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

    void drainAndFinalize()
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

    void finalize()
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

    mutable QMutex mu_;
    QWaitCondition cond_;
    QQueue<QueuedItem> queue_;
    quint64 queuedBytes_ = 0;

    bool recording_ = false;
    bool stopRequested_ = false;
    QString path_;
    QString lastError_;

    QElapsedTimer timer_;
    quint64 nextFrameIndex_ = 0;

    quint64 framesWritten_ = 0;
    quint64 durationMs_ = 0;
    quint64 bytesWritten_ = 0;
    quint64 droppedByBackpressure_ = 0;

    recording::FileHeader header_;

    mutable QMutex fileMu_;
    std::unique_ptr<QFile> file_;
};

FrameRecorder::FrameRecorder(QObject* parent)
    : QObject(parent)
{
    worker_ = new WriterWorker();
    thread_ = new QThread(this);
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &WriterWorker::writerLoop);
    connect(worker_, &WriterWorker::recordingStarted, this, &FrameRecorder::recordingStarted, Qt::QueuedConnection);
    connect(worker_, &WriterWorker::recordingStopped, this, &FrameRecorder::recordingStopped, Qt::QueuedConnection);
    connect(worker_, &WriterWorker::recordingError, this, &FrameRecorder::recordingError, Qt::QueuedConnection);
    connect(worker_, &WriterWorker::statsChanged, this, &FrameRecorder::statsChanged, Qt::QueuedConnection);

    thread_->start();
}

FrameRecorder::~FrameRecorder()
{
    if (worker_) {
        worker_->stop();
    }
    if (thread_) {
        const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + 2000;
        while (worker_ && isRecording() && QDateTime::currentMSecsSinceEpoch() < deadline) {
            QThread::msleep(20);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        thread_->quit();
        thread_->wait(2000);
    }
    if (worker_) {
        worker_->deleteLater();
        worker_ = nullptr;
    }
}

bool FrameRecorder::isRecording() const { return worker_ && worker_->isRecording(); }
QString FrameRecorder::filePath() const { return worker_ ? worker_->filePath() : QString(); }
quint64 FrameRecorder::framesWritten() const { return worker_ ? worker_->framesWritten() : 0; }
quint64 FrameRecorder::durationMs() const { return worker_ ? worker_->durationMs() : 0; }
quint64 FrameRecorder::bytesWritten() const { return worker_ ? worker_->bytesWritten() : 0; }
quint64 FrameRecorder::bytesQueued() const { return worker_ ? worker_->bytesQueued() : 0; }
quint64 FrameRecorder::droppedByBackpressure() const { return worker_ ? worker_->droppedByBackpressure() : 0; }

bool FrameRecorder::startRecording(const QString& path, QString* err)
{
    if (!worker_) return false;
    if (isRecording()) {
        if (err) *err = "already recording";
        return false;
    }
    worker_->start(path);
    return true;
}

bool FrameRecorder::stopRecording(int waitTimeoutMs, QString* err)
{
    if (!worker_) return true;
    if (!isRecording()) return true;

    worker_->stop();

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + waitTimeoutMs;
    while (isRecording() && QDateTime::currentMSecsSinceEpoch() < deadline) {
        QThread::msleep(20);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    if (isRecording()) {
        if (err) *err = "stop timeout";
        return false;
    }
    return true;
}

void FrameRecorder::recordFrame(int channel, int width, int height, int pixfmt, const QByteArray& data)
{
    if (!worker_) return;
    if (!isRecording()) return;
    worker_->enqueueFrame(channel, width, height, pixfmt, data);
}

#include "FrameRecorder.moc"

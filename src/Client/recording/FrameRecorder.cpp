#include "FrameRecorder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QThread>

#include "FrameRecorderWriterWorker.h"

FrameRecorder::FrameRecorder(QObject* parent)
    : QObject(parent)
{
    worker_ = new FrameRecorderWriterWorker();
    thread_ = new QThread(this);
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &FrameRecorderWriterWorker::writerLoop);
    connect(worker_, &FrameRecorderWriterWorker::recordingStarted, this, &FrameRecorder::recordingStarted, Qt::QueuedConnection);
    connect(worker_, &FrameRecorderWriterWorker::recordingStopped, this, &FrameRecorder::recordingStopped, Qt::QueuedConnection);
    connect(worker_, &FrameRecorderWriterWorker::recordingError, this, &FrameRecorder::recordingError, Qt::QueuedConnection);
    connect(worker_, &FrameRecorderWriterWorker::statsChanged, this, &FrameRecorder::statsChanged, Qt::QueuedConnection);

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

void FrameRecorder::recordFrame(int channel, int width, int height, int pixfmt, quint8 frameType,
                                quint64 streamFrameId, const QByteArray& data)
{
    if (!worker_) return;
    if (!isRecording()) return;
    worker_->enqueueFrame(channel, width, height, pixfmt, frameType, streamFrameId, data);
}

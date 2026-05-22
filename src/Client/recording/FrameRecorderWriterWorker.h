#pragma once

#include <memory>

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QWaitCondition>

#include "RecordingFileFormat.h"

class FrameRecorderWriterWorker : public QObject {
    Q_OBJECT
public:
    explicit FrameRecorderWriterWorker(QObject* parent = nullptr);

    bool isRecording() const;
    QString filePath() const;
    quint64 framesWritten() const;
    quint64 durationMs() const;
    quint64 bytesWritten() const;
    quint64 bytesQueued() const;
    quint64 droppedByBackpressure() const;

    void start(const QString& path);
    void stop();
    void enqueueFrame(int channel, int width, int height, int pixfmt, quint8 frameType,
                      quint64 streamFrameId, const QByteArray& data);
    void writerLoop();

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
        quint64 streamFrameId = 0;
        quint64 timestampMs = 0;
        quint8 frameType = 0;
        quint32 crc32 = 0;
        QByteArray data;
    };

    bool writeOne(const QueuedItem& item);
    void drainAndFinalize();
    void finalize();

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

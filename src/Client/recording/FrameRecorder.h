#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QThread;
class FrameRecorderWriterWorker;

class FrameRecorder : public QObject {
    Q_OBJECT
public:
    explicit FrameRecorder(QObject* parent = nullptr);
    ~FrameRecorder() override;

    bool isRecording() const;
    QString filePath() const;

    quint64 framesWritten() const;
    quint64 durationMs() const;
    quint64 bytesWritten() const;
    quint64 bytesQueued() const;
    quint64 droppedByBackpressure() const;

    bool startRecording(const QString& path, QString* err = nullptr);
    bool stopRecording(int waitTimeoutMs, QString* err = nullptr);

    void recordFrame(int channel, int width, int height, int pixfmt, quint8 frameType,
                     quint64 streamFrameId, const QByteArray& data);

signals:
    void recordingStarted(QString path);
    void recordingStopped(QString path, bool ok, QString err);
    void recordingError(QString err);
    void statsChanged();

private:
    FrameRecorderWriterWorker* worker_ = nullptr;
    QThread* thread_ = nullptr;
};

#pragma once

#include <QObject>
#include <QTimer>
#include <QSet>
#include <memory>

#include "RecordedFrame.h"
#include "RecordingFileFormat.h"

class QFile;
class QThread;
class FramePlaybackScanWorker;

class FramePlaybackController : public QObject {
    Q_OBJECT
public:
    explicit FramePlaybackController(QObject* parent = nullptr);
    ~FramePlaybackController() override;

    bool isScanning() const;
    bool isIndexReady() const;
    bool isPlaying() const;

    QString filePath() const;
    quint64 frameCount() const;
    quint64 durationMs() const;
    quint64 damagedFrames() const;
    bool wasClosedOk() const;

    int fps() const { return fps_; }
    quint64 playFrameLimit() const { return playFrameLimit_; } // 0 = to end
    bool loop() const { return loop_; }

    quint64 currentIndex() const { return currentIndex_; }

public slots:
    void openFile(const QString& path);
    void closeFile();

    void setFps(int fps);
    void setPlayFrameLimit(quint64 n);
    void setLoop(bool loop);

    void seekTo(quint64 index);

    void play();
    void pause();
    void stop();

signals:
    void scanStarted(QString path);
    void scanFinished(QString path, bool ok, QString err);
    void indexReady();
    void playbackStarted();
    void playbackStopped();
    void playbackPaused();
    void playbackPositionChanged(quint64 index);
    void playbackFrameReady(RecordedFrame frame);
    void playbackError(QString err);

private slots:
    void onTick();

private:
    struct IndexEntry {
        quint64 frameIndex = 0;
        quint64 timestampMs = 0;
        quint64 frameHeaderOffset = 0;
        quint64 payloadOffset = 0;
        quint32 payloadBytes = 0;
        quint32 crc32 = 0;
        quint8  channel = 0;
        quint16 width = 0;
        quint16 height = 0;
        quint16 pixfmt = 0;
    };

    friend class FramePlaybackScanWorker;

    bool loadFrameAt(quint64 idx, RecordedFrame& out, QString* err);
    void updateTimer();
    void autoAdvanceOrStop();

    QString path_;
    recording::FileHeader header_;
    QList<IndexEntry> index_;
    quint64 damagedFrames_ = 0;
    QSet<quint64> damagedIndexSet_;

    std::unique_ptr<QFile> file_;

    QThread* scanThread_ = nullptr;
    FramePlaybackScanWorker* scanWorker_ = nullptr;
    bool scanning_ = false;
    bool indexReady_ = false;

    QTimer tick_;
    bool playing_ = false;

    int fps_ = 25;
    quint64 playFrameLimit_ = 0;
    bool loop_ = false;

    quint64 currentIndex_ = 0;
    quint64 playedCount_ = 0; // played in current run (total)
    quint64 windowStartIndex_ = 0;
    quint64 windowFrameCount_ = 0;
};

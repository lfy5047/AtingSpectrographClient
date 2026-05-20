#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QSpinBox;
class QCheckBox;
class QSlider;
class QTimer;

class FrameRecorder;
class FramePlaybackController;
struct RecordedFrame;

class RecordPlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit RecordPlaybackPanel(QWidget* parent = nullptr);
    ~RecordPlaybackPanel() override;

    FrameRecorder* recorder() const { return recorder_; }
    FramePlaybackController* playback() const { return playback_; }

signals:
    void playbackStarted();
    void playbackFrameReady(RecordedFrame frame);
    void requestSwitchToPlaybackView();

private slots:
    void chooseRecordFile();
    void startRecording();
    void stopRecording();

    void choosePlaybackFile();
    void startPlayback();
    void pausePlayback();
    void stopPlayback();

    void onIndexReady();
    void onScanFinished(const QString& path, bool ok, const QString& err);
    void onPlaybackPositionChanged(quint64 idx);
    void onPlaybackFrameReady(RecordedFrame frame);
    void onPlaybackError(const QString& err);

    void onSliderReleased();
    void refreshStats();

private:
    void setStatus(const QString& text, bool isError = false);
    void updateUiEnabled();
    void updateSliderRange();

    FrameRecorder* recorder_ = nullptr;
    FramePlaybackController* playback_ = nullptr;

    // Recording UI
    QLineEdit* recordPathEdit_ = nullptr;
    QPushButton* recordBrowseBtn_ = nullptr;
    QPushButton* recordStartBtn_ = nullptr;
    QPushButton* recordStopBtn_ = nullptr;
    QLabel* recordFramesLbl_ = nullptr;
    QLabel* recordDurLbl_ = nullptr;
    QLabel* recordSizeLbl_ = nullptr;
    QLabel* recordFreeLbl_ = nullptr;
    QLabel* recordDropLbl_ = nullptr;

    // Playback UI
    QLineEdit* playbackPathEdit_ = nullptr;
    QPushButton* playbackBrowseBtn_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QSpinBox* playFramesSpin_ = nullptr;
    QCheckBox* loopChk_ = nullptr;
    QPushButton* playBtn_ = nullptr;
    QPushButton* pauseBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QSlider* progressSlider_ = nullptr;
    QLabel* playbackPosLbl_ = nullptr;
    QLabel* playbackInfoLbl_ = nullptr;
    QLabel* playbackDamagedLbl_ = nullptr;

    // Status
    QLabel* statusLbl_ = nullptr;
    QTimer* statsTimer_ = nullptr;

    bool sliderDragging_ = false;
};


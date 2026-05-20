#include "RecordPlaybackPanel.h"

#include "Client/recording/FrameRecorder.h"
#include "Client/recording/FramePlaybackController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTimer>
#include <QSettings>
#include <limits>

static QString formatBytes(quint64 bytes)
{
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    if (bytes >= static_cast<quint64>(gb)) return QString::number(bytes / gb, 'f', 2) + " GiB";
    if (bytes >= static_cast<quint64>(mb)) return QString::number(bytes / mb, 'f', 2) + " MiB";
    if (bytes >= static_cast<quint64>(kb)) return QString::number(bytes / kb, 'f', 1) + " KiB";
    return QString::number(bytes) + " B";
}

RecordPlaybackPanel::RecordPlaybackPanel(QWidget* parent)
    : QWidget(parent)
{
    qRegisterMetaType<RecordedFrame>("RecordedFrame");
    recorder_ = new FrameRecorder(this);
    playback_ = new FramePlaybackController(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    // ---- Recording ----
    {
        auto* grp = new QGroupBox(QString::fromUtf8("录制"), this);
        auto* fl = new QFormLayout(grp);

        auto* row = new QHBoxLayout();
        recordPathEdit_ = new QLineEdit(this);
        recordPathEdit_->setReadOnly(true);
        recordBrowseBtn_ = new QPushButton(QString::fromUtf8("浏览"), this);
        row->addWidget(recordPathEdit_, 1);
        row->addWidget(recordBrowseBtn_);
        fl->addRow(QString::fromUtf8("文件"), row);

        auto* btnRow = new QHBoxLayout();
        recordStartBtn_ = new QPushButton(QString::fromUtf8("开始录制"), this);
        recordStartBtn_->setProperty("primary", true);
        recordStopBtn_ = new QPushButton(QString::fromUtf8("停止录制"), this);
        recordStopBtn_->setProperty("danger", true);
        btnRow->addWidget(recordStartBtn_);
        btnRow->addWidget(recordStopBtn_);
        fl->addRow(btnRow);

        recordFramesLbl_ = new QLabel("0", this);
        recordDurLbl_ = new QLabel("0", this);
        recordSizeLbl_ = new QLabel("0", this);
        recordFreeLbl_ = new QLabel("-", this);
        recordDropLbl_ = new QLabel("0", this);

        fl->addRow(QString::fromUtf8("帧数"), recordFramesLbl_);
        fl->addRow(QString::fromUtf8("时长(ms)"), recordDurLbl_);
        fl->addRow(QString::fromUtf8("文件大小"), recordSizeLbl_);
        fl->addRow(QString::fromUtf8("磁盘剩余"), recordFreeLbl_);
        fl->addRow(QString::fromUtf8("背压丢弃"), recordDropLbl_);

        root->addWidget(grp);
    }

    // ---- Playback ----
    {
        auto* grp = new QGroupBox(QString::fromUtf8("回放"), this);
        auto* fl = new QFormLayout(grp);

        auto* row = new QHBoxLayout();
        playbackPathEdit_ = new QLineEdit(this);
        playbackPathEdit_->setReadOnly(true);
        playbackBrowseBtn_ = new QPushButton(QString::fromUtf8("浏览"), this);
        row->addWidget(playbackPathEdit_, 1);
        row->addWidget(playbackBrowseBtn_);
        fl->addRow(QString::fromUtf8("文件"), row);

        fpsSpin_ = new QSpinBox(this);
        fpsSpin_->setRange(1, 120);
        fpsSpin_->setValue(25);
        fl->addRow("FPS", fpsSpin_);

        playFramesSpin_ = new QSpinBox(this);
        playFramesSpin_->setRange(0, 1000000000);
        playFramesSpin_->setValue(0);
        fl->addRow(QString::fromUtf8("播放帧数(0=到末尾)"), playFramesSpin_);

        loopChk_ = new QCheckBox(QString::fromUtf8("循环播放"), this);
        fl->addRow(loopChk_);

        auto* btnRow = new QHBoxLayout();
        playBtn_ = new QPushButton(QString::fromUtf8("播放"), this);
        playBtn_->setProperty("primary", true);
        pauseBtn_ = new QPushButton(QString::fromUtf8("暂停"), this);
        stopBtn_ = new QPushButton(QString::fromUtf8("停止"), this);
        stopBtn_->setProperty("danger", true);
        btnRow->addWidget(playBtn_);
        btnRow->addWidget(pauseBtn_);
        btnRow->addWidget(stopBtn_);
        fl->addRow(btnRow);

        progressSlider_ = new QSlider(Qt::Horizontal, this);
        progressSlider_->setRange(0, 0);
        fl->addRow(QString::fromUtf8("进度"), progressSlider_);

        playbackPosLbl_ = new QLabel("0/0", this);
        playbackInfoLbl_ = new QLabel("-", this);
        playbackDamagedLbl_ = new QLabel("0", this);
        fl->addRow(QString::fromUtf8("当前位置"), playbackPosLbl_);
        fl->addRow(QString::fromUtf8("帧信息"), playbackInfoLbl_);
        fl->addRow(QString::fromUtf8("损坏帧"), playbackDamagedLbl_);

        root->addWidget(grp);
    }

    // ---- Status ----
    {
        auto* grp = new QGroupBox(QString::fromUtf8("状态"), this);
        auto* vb = new QVBoxLayout(grp);
        statusLbl_ = new QLabel("-", this);
        statusLbl_->setWordWrap(true);
        vb->addWidget(statusLbl_);
        root->addWidget(grp);
    }

    root->addStretch();

    connect(recordBrowseBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::chooseRecordFile);
    connect(recordStartBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::startRecording);
    connect(recordStopBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::stopRecording);

    connect(playbackBrowseBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::choosePlaybackFile);
    connect(playBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::startPlayback);
    connect(pauseBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::pausePlayback);
    connect(stopBtn_, &QPushButton::clicked, this, &RecordPlaybackPanel::stopPlayback);

    connect(progressSlider_, &QSlider::sliderPressed, this, [this]() { sliderDragging_ = true; });
    connect(progressSlider_, &QSlider::sliderReleased, this, &RecordPlaybackPanel::onSliderReleased);

    connect(playback_, &FramePlaybackController::indexReady, this, &RecordPlaybackPanel::onIndexReady);
    connect(playback_, &FramePlaybackController::scanFinished, this, &RecordPlaybackPanel::onScanFinished);
    connect(playback_, &FramePlaybackController::playbackPositionChanged, this, &RecordPlaybackPanel::onPlaybackPositionChanged);
    connect(playback_, &FramePlaybackController::playbackFrameReady, this, &RecordPlaybackPanel::onPlaybackFrameReady);
    connect(playback_, &FramePlaybackController::playbackError, this, &RecordPlaybackPanel::onPlaybackError);
    connect(playback_, &FramePlaybackController::playbackStarted, this, &RecordPlaybackPanel::updateUiEnabled);
    connect(playback_, &FramePlaybackController::playbackStopped, this, &RecordPlaybackPanel::updateUiEnabled);
    connect(playback_, &FramePlaybackController::playbackPaused, this, &RecordPlaybackPanel::updateUiEnabled);

    connect(recorder_, &FrameRecorder::recordingError, this, [this](const QString& e) { setStatus(e, true); });
    connect(recorder_, &FrameRecorder::recordingStopped, this, [this](const QString&, bool ok, const QString& e) {
        if (!ok) setStatus(e, true);
        updateUiEnabled();
    });
    connect(recorder_, &FrameRecorder::recordingStarted, this, [this](const QString&) { updateUiEnabled(); });

    statsTimer_ = new QTimer(this);
    statsTimer_->setInterval(500);
    connect(statsTimer_, &QTimer::timeout, this, &RecordPlaybackPanel::refreshStats);
    statsTimer_->start();

    // Load persisted settings
    {
        QSettings s;
        recordPathEdit_->setText(s.value("recording/lastPath", "").toString());
        playbackPathEdit_->setText(s.value("playback/lastPath", "").toString());
        fpsSpin_->setValue(s.value("playback/fps", fpsSpin_->value()).toInt());
        playFramesSpin_->setValue(s.value("playback/frameLimit", playFramesSpin_->value()).toInt());
        loopChk_->setChecked(s.value("playback/loop", false).toBool());
    }

    updateUiEnabled();
}

RecordPlaybackPanel::~RecordPlaybackPanel() = default;

void RecordPlaybackPanel::setStatus(const QString& text, bool isError)
{
    statusLbl_->setText(text.isEmpty() ? "-" : text);
    statusLbl_->setStyleSheet(isError ? "color:#E5484D;" : "");
}

void RecordPlaybackPanel::updateUiEnabled()
{
    const bool recording = recorder_->isRecording();
    const bool playing = playback_->isPlaying();
    const bool scanning = playback_->isScanning();

    recordBrowseBtn_->setEnabled(!recording && !playing);
    recordStartBtn_->setEnabled(!recording && !playing);
    recordStopBtn_->setEnabled(recording);

    playbackBrowseBtn_->setEnabled(!recording && !playing && !scanning);
    playBtn_->setEnabled(!recording && playback_->isIndexReady() && !playing);
    pauseBtn_->setEnabled(playing);
    stopBtn_->setEnabled(playing);

    fpsSpin_->setEnabled(!playing);
    playFramesSpin_->setEnabled(!playing);
    loopChk_->setEnabled(!playing);
    progressSlider_->setEnabled(playback_->isIndexReady() && !playing);
}

void RecordPlaybackPanel::updateSliderRange()
{
    const quint64 count = playback_->frameCount();
    if (count == 0) {
        progressSlider_->setRange(0, 0);
        return;
    }
    const int max = (count > static_cast<quint64>(std::numeric_limits<int>::max()))
        ? std::numeric_limits<int>::max()
        : static_cast<int>(count - 1);
    progressSlider_->setRange(0, qMax(0, max));
}

void RecordPlaybackPanel::chooseRecordFile()
{
    QDir().mkpath("recordings");
    const QString def = QString("recordings/record_%1.asrec")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("选择录制文件"), def,
                                                      "Ating Stream Recording (*.asrec)");
    if (path.isEmpty()) return;
    recordPathEdit_->setText(path);
    QSettings().setValue("recording/lastPath", path);
}

void RecordPlaybackPanel::startRecording()
{
    if (playback_->isPlaying() || playback_->isScanning()) return;
    QString path = recordPathEdit_->text();
    if (path.isEmpty()) {
        chooseRecordFile();
        path = recordPathEdit_->text();
        if (path.isEmpty()) return;
    }
    if (QFile::exists(path)) {
        if (QMessageBox::question(this, QString::fromUtf8("录制"),
            QString::fromUtf8("文件已存在，是否覆盖？")) != QMessageBox::Yes) return;
        QFile::remove(path);
    }
    QString err;
    if (!recorder_->startRecording(path, &err)) {
        setStatus(err, true);
        return;
    }
    setStatus(QString::fromUtf8("录制中..."), false);
    updateUiEnabled();
}

void RecordPlaybackPanel::stopRecording()
{
    QString err;
    if (!recorder_->stopRecording(3000, &err)) {
        setStatus(err, true);
    }
    updateUiEnabled();
}

void RecordPlaybackPanel::choosePlaybackFile()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择回放文件"), QString(),
                                                      "Ating Stream Recording (*.asrec)");
    if (path.isEmpty()) return;
    playbackPathEdit_->setText(path);
    QSettings().setValue("playback/lastPath", path);
    setStatus(QString::fromUtf8("正在扫描索引..."), false);
    playback_->openFile(path);
    updateUiEnabled();
}

void RecordPlaybackPanel::startPlayback()
{
    if (!playback_->isIndexReady()) return;
    playback_->setFps(fpsSpin_->value());
    playback_->setPlayFrameLimit(static_cast<quint64>(playFramesSpin_->value()));
    playback_->setLoop(loopChk_->isChecked());
    {
        QSettings s;
        s.setValue("playback/fps", fpsSpin_->value());
        s.setValue("playback/frameLimit", playFramesSpin_->value());
        s.setValue("playback/loop", loopChk_->isChecked());
    }

    emit requestSwitchToPlaybackView();
    playback_->play();
    emit playbackStarted();
    updateUiEnabled();
}

void RecordPlaybackPanel::pausePlayback()
{
    playback_->pause();
    updateUiEnabled();
}

void RecordPlaybackPanel::stopPlayback()
{
    playback_->stop();
    updateUiEnabled();
}

void RecordPlaybackPanel::onIndexReady()
{
    updateSliderRange();
    setStatus(QString::fromUtf8("索引就绪"), false);
    updateUiEnabled();
}

void RecordPlaybackPanel::onScanFinished(const QString&, bool ok, const QString& err)
{
    if (!ok) setStatus(err, true);
    updateUiEnabled();
}

void RecordPlaybackPanel::onPlaybackPositionChanged(quint64 idx)
{
    if (!playback_->isIndexReady()) return;
    const quint64 count = playback_->frameCount();
    playbackPosLbl_->setText(QString("%1/%2").arg(idx < count ? idx + 1 : count).arg(count));
    if (!sliderDragging_ && idx <= static_cast<quint64>(progressSlider_->maximum())) {
        progressSlider_->setValue(static_cast<int>(idx));
    }
}

void RecordPlaybackPanel::onPlaybackFrameReady(RecordedFrame frame)
{
    playbackDamagedLbl_->setText(QString::number(playback_->damagedFrames()));
    playbackInfoLbl_->setText(QString("ch=%1 %2x%3 pixfmt=%4")
        .arg(frame.channel).arg(frame.width).arg(frame.height).arg(frame.pixfmt));
    emit playbackFrameReady(frame);
}

void RecordPlaybackPanel::onPlaybackError(const QString& err)
{
    if (!err.isEmpty()) setStatus(err, true);
    playbackDamagedLbl_->setText(QString::number(playback_->damagedFrames()));
}

void RecordPlaybackPanel::onSliderReleased()
{
    sliderDragging_ = false;
    if (!playback_->isIndexReady()) return;
    playback_->seekTo(static_cast<quint64>(progressSlider_->value()));
}

void RecordPlaybackPanel::refreshStats()
{
    // Recording stats
    recordFramesLbl_->setText(QString::number(recorder_->framesWritten()));
    recordDurLbl_->setText(QString::number(recorder_->durationMs()));
    recordSizeLbl_->setText(formatBytes(recorder_->bytesWritten()));
    recordDropLbl_->setText(QString::number(recorder_->droppedByBackpressure()));

    const QString path = recorder_->filePath().isEmpty() ? recordPathEdit_->text() : recorder_->filePath();
    if (!path.isEmpty()) {
        QStorageInfo si(QFileInfo(path).absolutePath());
        recordFreeLbl_->setText(si.isValid() ? formatBytes(static_cast<quint64>(si.bytesAvailable())) : "-");
    }

    // Playback stats
    playbackDamagedLbl_->setText(QString::number(playback_->damagedFrames()));
}

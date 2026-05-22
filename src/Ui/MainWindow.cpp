#include "MainWindow.h"
#include "DeviceClient.h"
#include "widgets/SidebarWidget.h"
#include "widgets/TopBarWidget.h"
#include "widgets/ImageView.h"
#include "panels/DashboardPanel.h"
#include "panels/ConnectionPanel.h"
#include "panels/CameraPanel.h"
#include "panels/MirrorPanel.h"
#include "panels/IrPanel.h"
#include "panels/CollectPanel.h"
#include "panels/StreamPanel.h"
#include "panels/SpectralPanel.h"
#include "panels/RecordPlaybackPanel.h"
#include "Client/recording/FrameRecorder.h"
#include "Client/recording/FramePlaybackController.h"
#include "Client/recording/RecordedFrame.h"
#include "panels/LogPanel.h"
#include "Protocol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>
#include <QCloseEvent>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QEvent>
#include <QApplication>
#include <QStatusBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("AtingSpectrograph Client — Spectra Pro");
    resize(1480, 880);
    setMinimumSize(1000, 600);

    device_ = new DeviceClient(this);
    setupUi();
    setupPanels();
    statusBar()->hide();
    setupConnections();
    loadSettings();
    uptime_.start();

    // uptime update timer
    auto* uptimeTimer = new QTimer(this);
    connect(uptimeTimer, &QTimer::timeout, this, [this]() {
        qint64 secs = uptime_.elapsed() / 1000;
        int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
        dashPanel_->setUptime(QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0')));
    });
    uptimeTimer->start(1000);

    spectralRenderTimer_ = new QTimer(this);
    spectralRenderTimer_->setInterval(33);
    connect(spectralRenderTimer_, &QTimer::timeout, this, [this]() {
        if (!spectralDirty_) return;
        updateSpectralView();
        spectralDirty_ = false;
    });
    spectralRenderTimer_->start();

    refreshSpectralStats();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (recordPlaybackPanel_ && recordPlaybackPanel_->recorder() &&
        recordPlaybackPanel_->recorder()->isRecording()) {
        const auto ret = QMessageBox::question(
            this,
            QString::fromUtf8("录制"),
            QString::fromUtf8("正在录制中，是否停止录制并退出？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            e->ignore();
            return;
        }
        QString err;
        if (!recordPlaybackPanel_->recorder()->stopRecording(5000, &err)) {
            QMessageBox::warning(this, QString::fromUtf8("录制"), err);
        }
    }
    if (recordPlaybackPanel_ && recordPlaybackPanel_->playback()) {
        recordPlaybackPanel_->playback()->stop();
    }
    saveSettings();
    device_->disconnect();
    QMainWindow::closeEvent(e);
}

// ── Layout ──

static QScrollArea* wrapInScroll(QWidget* w)
{
    auto* area = new QScrollArea;
    area->setObjectName("panelScroll");
    area->setWidgetResizable(true);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(w);
    return area;
}

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Sidebar ──
    sidebar_ = new SidebarWidget(this);
    root->addWidget(sidebar_);

    // ── Right side ──
    auto* rightSide = new QVBoxLayout();
    rightSide->setContentsMargins(0, 0, 0, 0);
    rightSide->setSpacing(0);

    // TopBar
    topBar_ = new TopBarWidget(this);
    rightSide->addWidget(topBar_);

    // Content splitter
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);

    // ── Viewer area ──
    viewerContainer_ = new QWidget(this);
    viewerContainer_->setObjectName("viewerContainer");
    auto* viewerLayout = new QVBoxLayout(viewerContainer_);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);

    // Channel tab bar (above viewer)
    channelTabBar_ = new QWidget(viewerContainer_);
    channelTabBar_->setObjectName("channelTabBar");
    channelTabBar_->setFixedHeight(36);
    auto* chTabLayout = new QHBoxLayout(channelTabBar_);
    chTabLayout->setContentsMargins(0, 0, 0, 0);
    chTabLayout->setSpacing(2);
    chTabLayout->addStretch();

    chTabRaw16_ = new QPushButton("Raw16", channelTabBar_);
    chTabRaw16_->setObjectName("channelTab");
    chTabRaw16_->setFixedHeight(28);
    chTabRaw16_->setCursor(Qt::PointingHandCursor);
    chTabSlice_ = new QPushButton("SliceStitch16", channelTabBar_);
    chTabSlice_->setObjectName("channelTab");
    chTabSlice_->setFixedHeight(28);
    chTabSlice_->setCursor(Qt::PointingHandCursor);
    chTabSpectral_ = new QPushButton("Spectral", channelTabBar_);
    chTabSpectral_->setObjectName("channelTab");
    chTabSpectral_->setFixedHeight(28);
    chTabSpectral_->setCursor(Qt::PointingHandCursor);
    chTabPlayback_ = new QPushButton("Playback", channelTabBar_);
    chTabPlayback_->setObjectName("channelTab");
    chTabPlayback_->setFixedHeight(28);
    chTabPlayback_->setCursor(Qt::PointingHandCursor);

    chTabLayout->addWidget(chTabRaw16_);
    chTabLayout->addWidget(chTabSlice_);
    chTabLayout->addWidget(chTabSpectral_);
    chTabLayout->addWidget(chTabPlayback_);
    chTabLayout->addStretch();

    // Position channel tabs at top-center of viewer
    viewerLayout->addWidget(channelTabBar_);
    chTabRaw16_->setProperty("active", true);
    chTabRaw16_->style()->polish(chTabRaw16_);
    viewerLayout->addSpacing(10);

    // Stacked image views
    viewerStack_ = new QStackedWidget(viewerContainer_);

    imageViewRaw_ = new ImageView(viewerStack_);
    imageViewSlice_ = new ImageView(viewerStack_);
    imageViewSpectral_ = new ImageView(viewerStack_);
    imageViewPlayback_ = new ImageView(viewerStack_);
    viewerStack_->addWidget(imageViewRaw_);   // 0
    viewerStack_->addWidget(imageViewSlice_); // 1
    viewerStack_->addWidget(imageViewSpectral_); // 2
    viewerStack_->addWidget(imageViewPlayback_); // 3

    viewerLayout->addWidget(viewerStack_, 1);

    connect(chTabRaw16_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 0;
        viewerStack_->setCurrentIndex(0);
        updateChannelTabStyle(0);
    });
    connect(chTabSlice_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 1;
        viewerStack_->setCurrentIndex(1);
        updateChannelTabStyle(1);
    });
    connect(chTabSpectral_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 2;
        viewerStack_->setCurrentIndex(2);
        updateChannelTabStyle(2);
    });
    connect(chTabPlayback_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 3;
        viewerStack_->setCurrentIndex(3);
        updateChannelTabStyle(3);
    });

    // Zoom toolbar (overlay at bottom-center)
    zoomBar_ = new QWidget(viewerContainer_);
    zoomBar_->setObjectName("zoomBar");
    zoomBar_->setFixedHeight(38);
    auto* zoomLayout = new QHBoxLayout(zoomBar_);
    zoomLayout->setContentsMargins(8, 0, 8, 0);
    zoomLayout->setSpacing(3);
    zoomLayout->addStretch();

    // imgInfoLabel_ = new QLabel("--", zoomBar_);
    // imgInfoLabel_->setObjectName("imgInfoLabel");

    // zoomLayout->addWidget(imgInfoLabel_);
    // zoomLayout->addStretch();

    mainSplitter_->addWidget(viewerContainer_);

    // ── Right Panel ──
    rightPanel_ = new QWidget(this);
    rightPanel_->setObjectName("panelContainer");
    rightPanel_->setMinimumWidth(280);
    rightPanel_->setMaximumWidth(420);
    auto* panelLayout = new QVBoxLayout(rightPanel_);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    panelTitle_ = new QLabel(QString::fromUtf8("仪表盘"), rightPanel_);
    panelTitle_->setObjectName("panelTitle");
    panelTitle_->setFixedHeight(46);
    panelLayout->addWidget(panelTitle_);

    panelStack_ = new QStackedWidget(rightPanel_);
    panelStack_->setObjectName("panelStack");
    panelLayout->addWidget(panelStack_, 1);

    mainSplitter_->addWidget(rightPanel_);
    mainSplitter_->setStretchFactor(0, 3);
    mainSplitter_->setStretchFactor(1, 1);

    rightSide->addWidget(mainSplitter_, 1);

    // ── Bottom Log Panel ──
    logPanel_ = new LogPanel(this);
    logPanel_->setFixedHeight(36);
    rightSide->addWidget(logPanel_);

    root->addLayout(rightSide, 1);

    // Resize event to reposition overlays
    viewerContainer_->installEventFilter(this);
}

// Override event filter to reposition overlays on resize
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == viewerContainer_ && event->type() == QEvent::Resize) {
        int w = viewerContainer_->width();
        zoomBar_->setFixedWidth(w);
        zoomBar_->move(0, viewerContainer_->height() - 48);
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::updateChannelTabStyle(int tab)
{
    chTabRaw16_->setProperty("active", tab == 0);
    chTabRaw16_->style()->polish(chTabRaw16_);
    chTabSlice_->setProperty("active", tab == 1);
    chTabSlice_->style()->polish(chTabSlice_);
    chTabSpectral_->setProperty("active", tab == 2);
    chTabSpectral_->style()->polish(chTabSpectral_);
    chTabPlayback_->setProperty("active", tab == 3);
    chTabPlayback_->style()->polish(chTabPlayback_);
}

void MainWindow::renderFrameToView(ImageView* target, int width, int height, int pixfmt, const QByteArray& data)
{
    using namespace cli::proto;

    if (!target) return;

    QImage img;
    if (pixfmt == Mono8) {
        if (data.size() < width * height) return;
        img = QImage(reinterpret_cast<const uchar*>(data.constData()),
                     width, height, width, QImage::Format_Grayscale8).copy();
    } else {
        const int pixels = width * height;
        if (data.size() < pixels * 2) return;
        const uint16_t* src = reinterpret_cast<const uint16_t*>(data.constData());
        uint16_t mn = 0xFFFF, mx = 0;
        for (int i = 0; i < pixels; ++i) {
            if (src[i] < mn) mn = src[i];
            if (src[i] > mx) mx = src[i];
        }
        QByteArray buf8(pixels, '\0');
        if (mx > mn) {
            const double s = 255.0 / (mx - mn);
            for (int i = 0; i < pixels; ++i)
                buf8[i] = static_cast<char>(static_cast<uint8_t>((src[i] - mn) * s));
        }
        img = QImage(reinterpret_cast<const uchar*>(buf8.constData()),
                     width, height, width, QImage::Format_Grayscale8).copy();
    }

    target->setImage(img);
}

// ── Panels ──

static const char* kPanelNames[] = {
    "仪表盘",
    "设备连接",
    "相机设置",
    "转镜控制",
    "红外热像",
    "数据采集",
    "流通道",
    "光谱显示",
    "录制回放",
    "系统日志",
};

void MainWindow::setupPanels()
{
    dashPanel_    = new DashboardPanel(this);
    connPanel_    = new ConnectionPanel(device_, this);
    cameraPanel_  = new CameraPanel(device_, this);
    mirrorPanel_  = new MirrorPanel(device_, this);
    irPanel_      = new IrPanel(device_, this);
    collectPanel_ = new CollectPanel(device_, this);
    streamPanel_  = new StreamPanel(device_, this);
    spectralPanel_ = new SpectralPanel(this);
    recordPlaybackPanel_ = new RecordPlaybackPanel(this);

    // Panel 0: Dashboard (no scroll wrap — its own internal scroll)
    panelStack_->addWidget(wrapInScroll(dashPanel_));
    // Panel 1-7: control panels
    panelStack_->addWidget(wrapInScroll(connPanel_));
    panelStack_->addWidget(wrapInScroll(cameraPanel_));
    panelStack_->addWidget(wrapInScroll(mirrorPanel_));
    panelStack_->addWidget(wrapInScroll(irPanel_));
    panelStack_->addWidget(wrapInScroll(collectPanel_));
    panelStack_->addWidget(wrapInScroll(streamPanel_));
    panelStack_->addWidget(wrapInScroll(spectralPanel_));
    panelStack_->addWidget(wrapInScroll(recordPlaybackPanel_));
    // Panel 9: Logs — jumps to bottom log panel
}

void MainWindow::onPanelSelected(int index)
{
    if (index < 0 || index > 9) return;

    // Panel 9 (logs) toggles bottom log panel expansion
    if (index == 9) {
        logPanel_->toggleExpanded();
        return;
    }

    panelStack_->setCurrentIndex(index);
    panelTitle_->setText(QString::fromUtf8(kPanelNames[index]));

    if (index == 0) {
        // Update dashboard info
        dashPanel_->setConnectionInfo(hostIp_, tcpPort_, udpPort_,
                                       device_->isConnected(), deviceVersion_);
    }
}

// ── Connections ──

void MainWindow::setupConnections()
{
    // Sidebar → panel switch
    connect(sidebar_, &SidebarWidget::panelSelected, this, &MainWindow::onPanelSelected);

    // Connection changed
    connect(device_, &DeviceClient::connectionChanged, this,
            [this](bool connected, const QString& ip) {
        hostIp_ = connected ? ip : QString();
        sidebar_->setConnected(connected);
        topBar_->setConnected(connected, ip);

        if (connected) {
            quint16 port = connPanel_->udpPort();
            udpPort_ = port;
            streamPanel_->setUdpPort(port);
            if (!device_->stream()->isBound())
                device_->stream()->bind(port);

            // Query version
            device_->systemApi()->version(this, [this](bool ok, int ver, const QString& name, const QString&) {
                if (ok) {
                    deviceVersion_ = QString("%1 v%2").arg(name).arg(ver);
                    dashPanel_->setConnectionInfo(hostIp_, tcpPort_, udpPort_,
                                                   true, deviceVersion_);
                }
            });
        }

        dashPanel_->setConnectionInfo(hostIp_, tcpPort_, udpPort_, connected, deviceVersion_);
    });

    // Mirror angle event → TopBar + Dashboard + MirrorPanel (handled internally)
    connect(device_, &DeviceClient::mirrorAngleEvent, this,
            [this](double angle, bool moving, qint64) {
        topBar_->setMirrorAngle(angle);
        dashPanel_->setMirrorInfo(angle, moving);
    });

    // Frame ready → image viewer + stats
    connect(device_, &DeviceClient::frameReady, this,
            [this](const StreamFrame& frame) {
        using namespace cli::proto;

        if (recordPlaybackPanel_ && recordPlaybackPanel_->recorder()) {
            recordPlaybackPanel_->recorder()->recordFrame(frame.channel, frame.width, frame.height,
                                                          frame.pixfmt, frame.frameType,
                                                          frame.streamFrameId, frame.data);
        }

        handleLiveFrame(frame);

        ImageView* target = nullptr;
        switch (frame.channel) {
        case Raw16:          target = imageViewRaw_; break;
        case SliceStitch16:  target = imageViewSlice_; break;
        default: break;
        }
        if (target) {
            renderFrameToView(target, frame.width, frame.height, frame.pixfmt, frame.data);
        }

        refreshStreamStats();
    }, Qt::QueuedConnection);

    connect(device_->stream(), &StreamClient::statsUpdated, this, &MainWindow::refreshStreamStats);

    // Raw log → bottom log panel
    connect(device_->control(), &ControlClient::rawLog, this,
            [this](const QString& line) {
        logPanel_->appendLog(line);
    });

    // Stream panel → subscribe
    connect(streamPanel_, &StreamPanel::subscribeRequested, this,
            [this](quint16 port, const QStringList&) {
        if (!device_->stream()->isBound())
            device_->stream()->bind(port);
    });

    // Record/Playback panel -> switch viewer to Playback
    connect(recordPlaybackPanel_, &RecordPlaybackPanel::requestSwitchToPlaybackView, this, [this]() {
        currentChannel_ = 3;
        viewerStack_->setCurrentIndex(3);
        updateChannelTabStyle(3);
    });

    // Playback frames -> Playback ImageView
    connect(recordPlaybackPanel_, &RecordPlaybackPanel::playbackFrameReady, this,
            [this](const RecordedFrame& f) {
        handlePlaybackFrame(f);
        renderFrameToView(imageViewPlayback_, f.width, f.height, f.pixfmt, f.data);
    }, Qt::QueuedConnection);

    if (recordPlaybackPanel_ && recordPlaybackPanel_->playback()) {
        connect(recordPlaybackPanel_->playback(), &FramePlaybackController::playbackStarted, this, [this]() {
            playbackActive_ = true;
            playbackRawSpectral_.reset();
            playbackSliceSpectral_.reset();
            refreshSpectralSource();
        });
        connect(recordPlaybackPanel_->playback(), &FramePlaybackController::playbackStopped, this, [this]() {
            playbackActive_ = false;
            refreshSpectralSource();
            spectralDirty_ = true;
            refreshSpectralStats();
        });
    }

    connect(spectralPanel_, &SpectralPanel::settingsChanged, this, [this]() {
        refreshSpectralSource();
        spectralDirty_ = true;
        updateSpectralView();
        refreshStreamStats();
    });
}

// ── Settings ──

void MainWindow::loadSettings()
{
    QSettings s;
    restoreGeometry(s.value("window/geometry").toByteArray());
    if (mainSplitter_)
        mainSplitter_->restoreState(s.value("window/splitter").toByteArray());

    int panel = s.value("window/panel", 0).toInt();
    const int panelVersion = s.value("window/panelVersion", 1).toInt();
    if (panelVersion < 2) {
        // Migration: old index 7 = record/playback, old index 8 = logs.
        if (panel == 7) panel = 8;
        else if (panel == 8) panel = 9;
    }

    if (panel >= 0 && panel <= 9) {
        sidebar_->setActivePanel(panel);
        onPanelSelected(panel);
    }

    bool sidebarCollapsed = s.value("window/sidebarCollapsed", false).toBool();
    sidebar_->setCollapsed(sidebarCollapsed);
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue("window/geometry", saveGeometry());
    if (mainSplitter_)
        s.setValue("window/splitter", mainSplitter_->saveState());
    s.setValue("window/panel", sidebar_ ? sidebar_->activePanel() : panelStack_->currentIndex());
    s.setValue("window/panelVersion", 2);
    s.setValue("window/sidebarCollapsed", sidebar_->isCollapsed());
}

SpectralSource MainWindow::effectiveSpectralSource() const
{
    if (!spectralPanel_) return SpectralSource::Live;

    switch (spectralPanel_->sourceMode()) {
    case SpectralSourceMode::Live:
        return SpectralSource::Live;
    case SpectralSourceMode::Playback:
        return SpectralSource::Playback;
    case SpectralSourceMode::Auto:
    default:
        return playbackActive_ ? SpectralSource::Playback : SpectralSource::Live;
    }
}

void MainWindow::refreshSpectralSource()
{
    spectralSource_ = effectiveSpectralSource();
    spectralDirty_ = true;
    refreshSpectralStats();
}

SpectralScanBuilder* MainWindow::builderFor(SpectralSource source, int channel)
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

const SpectralScanBuilder* MainWindow::builderFor(SpectralSource source, int channel) const
{
    return const_cast<MainWindow*>(this)->builderFor(source, channel);
}

void MainWindow::refreshSpectralStats()
{
    if (!spectralPanel_) return;
    const int channel = spectralPanel_->sourceChannel();
    const SpectralScanBuilder* b = builderFor(spectralSource_, channel);
    const QString activeSource = spectralSource_ == SpectralSource::Live ? "Live" : "Playback";
    if (!b) {
        spectralPanel_->setBandCount(0);
        spectralPanel_->setStats(activeSource, 0, 0, 0, false, false, 0);
        return;
    }

    spectralPanel_->setBandCount(b->bands());
    spectralPanel_->setStats(activeSource, b->scanWidth(), b->height(), b->bands(),
                             b->tailSeen(), b->hasActiveScan(), b->gapFillColumns());
}

void MainWindow::handleLiveFrame(const StreamFrame& frame)
{
    using namespace cli::proto;
    if (frame.frameType != HeaderFrame && frame.frameType != DataFrame && frame.frameType != TailFrame) {
        return;
    }

    SpectralScanBuilder* b = builderFor(SpectralSource::Live, frame.channel);
    if (!b) return;
    b->feedFrame(frame.frameType, frame.streamFrameId, frame.width, frame.height, frame.pixfmt, frame.data);
    refreshSpectralStats();
    if (spectralSource_ == SpectralSource::Live) {
        spectralDirty_ = true;
    }
}

void MainWindow::handlePlaybackFrame(const RecordedFrame& frame)
{
    using namespace cli::proto;
    if (frame.frameType != HeaderFrame && frame.frameType != DataFrame && frame.frameType != TailFrame) {
        return;
    }

    SpectralScanBuilder* b = builderFor(SpectralSource::Playback, frame.channel);
    if (!b) return;
    b->feedFrame(frame.frameType, frame.streamFrameId, frame.width, frame.height, frame.pixfmt, frame.data);
    refreshSpectralStats();
    if (spectralSource_ == SpectralSource::Playback) {
        spectralDirty_ = true;
    }
}

void MainWindow::refreshStreamStats()
{
    if (!device_ || !device_->stream() || !topBar_ || !dashPanel_) return;

    using namespace cli::proto;
    int fpsChannel = 0;
    if (currentChannel_ == 0) {
        fpsChannel = Raw16;
    } else if (currentChannel_ == 1) {
        fpsChannel = SliceStitch16;
    } else if (currentChannel_ == 2 && spectralPanel_) {
        fpsChannel = spectralPanel_->sourceChannel();
    }

    const double selectedFps = fpsChannel > 0 ? device_->stream()->fps(fpsChannel) : device_->stream()->fps();
    topBar_->setFps(selectedFps);
    topBar_->setFrames(device_->stream()->framesReceived());
    topBar_->setDropped(device_->stream()->framesDropped());

    dashPanel_->setStreamStats(
        device_->stream()->fps(Raw16),
        device_->stream()->fps(SliceStitch16),
        device_->stream()->framesReceived(),
        device_->stream()->framesDropped());
}

void MainWindow::updateSpectralView()
{
    if (!imageViewSpectral_ || !spectralPanel_) return;

    const int channel = spectralPanel_->sourceChannel();
    const SpectralScanBuilder* b = builderFor(spectralSource_, channel);
    refreshSpectralStats();
    if (!b || !b->hasData()) {
        imageViewSpectral_->setNoSignal();
        return;
    }

    const SpectralRenderOptions opts = spectralPanel_->renderOptions();
    const QImage img = b->render(opts);
    if (img.isNull()) {
        imageViewSpectral_->setNoSignal();
        return;
    }

    imageViewSpectral_->setImage(img);
}

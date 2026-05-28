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
#include "panels/SpectrumAnalysisPanel.h"
#include "SpectrumCurveDialog.h"
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
#include <QProgressBar>
#include <QTimer>
#include <QEvent>
#include <QApplication>
#include <QStatusBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <cmath>
#include "plog/Log.h"

namespace {
const int kSpectrumAnalysisPanelIndex = 9;
const int kLogPanelIndex = 10;

struct Mono16Stats {
    quint16 min = 0;
    quint16 max = 0;
    quint64 sum = 0;
};

bool computeMono16Stats(const QByteArray& data, int width, int height, Mono16Stats* out)
{
    if (!out) return false;
    const int pixels = width * height;
    if (width <= 0 || height <= 0 || data.size() < pixels * 2) return false;

    const auto* src = reinterpret_cast<const uint16_t*>(data.constData());
    quint16 mn = 0xFFFF;
    quint16 mx = 0;
    quint64 sum = 0;
    for (int i = 0; i < pixels; ++i) {
        const quint16 v = src[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
    }

    out->min = mn;
    out->max = mx;
    out->sum = sum;
    return true;
}

QVector<int> spectrumSampledXs(int startX, int endX, int maxPlotPoints)
{
    QVector<int> xs;
    const int total = endX - startX + 1;
    if (total <= 0) return xs;

    maxPlotPoints = qBound(2, maxPlotPoints, total);
    xs.reserve(maxPlotPoints);
    if (total <= maxPlotPoints) {
        for (int x = startX; x <= endX; ++x) {
            xs.append(x);
        }
        return xs;
    }

    const double step = static_cast<double>(endX - startX) / static_cast<double>(maxPlotPoints - 1);
    for (int i = 0; i < maxPlotPoints; ++i) {
        int x = static_cast<int>(std::round(startX + i * step));
        if (i == 0) x = startX;
        if (i == maxPlotPoints - 1) x = endX;
        xs.append(qBound(startX, x, endX));
    }
    return xs;
}

double spectrumMovingAverage(const quint16* pixels, int rowOffset, int width, int x, int windowPixels)
{
    windowPixels = qMax(1, windowPixels);
    const int halfWindow = windowPixels / 2;
    const int left = qMax(0, x - halfWindow);
    const int right = qMin(width - 1, x + halfWindow);

    quint64 sum = 0;
    for (int sx = left; sx <= right; ++sx) {
        sum += pixels[rowOffset + sx];
    }
    return static_cast<double>(sum) / static_cast<double>(right - left + 1);
}

int spectralProgressIndex(SpectralSource source, int channel)
{
    using namespace cli::proto;
    const int sourceIndex = source == SpectralSource::Live ? 0 : 1;
    int channelIndex = -1;
    if (channel == Raw16) channelIndex = 0;
    else if (channel == SliceStitch16) channelIndex = 1;
    if (channelIndex < 0) return -1;
    return sourceIndex * 2 + channelIndex;
}
} // namespace

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
        if (!spectralDirty_ || currentChannel_ != 2) return;
        updateSpectralView();
        spectralDirty_ = false;
    });
    spectralRenderTimer_->start();

    spectralProgressTimer_ = new QTimer(this);
    spectralProgressTimer_->setInterval(100);
    connect(spectralProgressTimer_, &QTimer::timeout, this, [this]() {
        refreshSpectralProgressOverlay();
    });
    spectralProgressTimer_->start();

    refreshSpectralStats();
    refreshImageStatsOverlay();
    refreshSpectralProgressOverlay();
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
        refreshImageStatsOverlay();
        refreshSpectralProgressOverlay();
    });
    connect(chTabSlice_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 1;
        viewerStack_->setCurrentIndex(1);
        updateChannelTabStyle(1);
        refreshImageStatsOverlay();
        refreshSpectralProgressOverlay();
    });
    connect(chTabSpectral_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 2;
        viewerStack_->setCurrentIndex(2);
        updateChannelTabStyle(2);
        refreshSpectralSource();
        refreshSpectralStats();
        refreshImageStatsOverlay();
        refreshSpectralProgressOverlay();
    });
    connect(chTabPlayback_, &QPushButton::clicked, this, [this]() {
        currentChannel_ = 3;
        viewerStack_->setCurrentIndex(3);
        updateChannelTabStyle(3);
        refreshImageStatsOverlay();
        refreshSpectralProgressOverlay();
    });

    // Zoom toolbar (kept hidden for now)
    zoomBar_ = new QWidget(viewerContainer_);
    zoomBar_->setObjectName("zoomBar");
    zoomBar_->setFixedHeight(38);
    zoomBar_->setAttribute(Qt::WA_TransparentForMouseEvents);
    zoomBar_->hide();
    auto* zoomLayout = new QHBoxLayout(zoomBar_);
    zoomLayout->setContentsMargins(8, 0, 8, 0);
    zoomLayout->setSpacing(3);
    zoomLayout->addStretch();

    // Right-bottom stats overlay
    imageStatsOverlay_ = new QWidget(viewerContainer_);
    imageStatsOverlay_->setObjectName("imageStatsOverlay");
    imageStatsOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* statsLayout = new QVBoxLayout(imageStatsOverlay_);
    statsLayout->setContentsMargins(10, 8, 10, 8);
    statsLayout->setSpacing(0);
    imageStatsLabel_ = new QLabel(imageStatsOverlay_);
    imageStatsLabel_->setObjectName("imgInfoLabel");
    imageStatsLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    imageStatsLabel_->setText(QString::fromUtf8("x: --  y: --\nmin: --  max: --  avg: --"));
    statsLayout->addWidget(imageStatsLabel_);
    imageStatsOverlay_->adjustSize();
    imageStatsOverlay_->raise();

    spectralProgressOverlay_ = new QWidget(viewerContainer_);
    spectralProgressOverlay_->setObjectName("spectralProgressOverlay");
    spectralProgressOverlay_->setAttribute(Qt::WA_TransparentForMouseEvents);
    spectralProgressOverlay_->setMinimumWidth(300);
    auto* progressLayout = new QVBoxLayout(spectralProgressOverlay_);
    progressLayout->setContentsMargins(12, 8, 12, 8);
    progressLayout->setSpacing(6);
    spectralProgressLabel_ = new QLabel(spectralProgressOverlay_);
    spectralProgressLabel_->setObjectName("spectralProgressLabel");
    spectralProgressLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    spectralProgressLabel_->setText(QString::fromUtf8("等待整帧完整数据"));
    spectralProgressBar_ = new QProgressBar(spectralProgressOverlay_);
    spectralProgressBar_->setObjectName("spectralProgressBar");
    spectralProgressBar_->setMinimumWidth(276);
    spectralProgressBar_->setRange(0, 100);
    spectralProgressBar_->setValue(0);
    spectralProgressBar_->setTextVisible(false);
    progressLayout->addWidget(spectralProgressLabel_);
    progressLayout->addWidget(spectralProgressBar_);
    spectralProgressOverlay_->adjustSize();
    spectralProgressOverlay_->hide();
    spectralProgressOverlay_->raise();

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
    panelTitle_->installEventFilter(this);
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
        positionImageStatsOverlay();
        positionSpectralProgressOverlay();
    } else if (obj == panelTitle_ && event->type() == QEvent::MouseButtonRelease) {
        if (sidebar_ && sidebar_->activePanel() == kSpectrumAnalysisPanelIndex) {
            openSpectrumAnalysisDialog();
            return true;
        }
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
    refreshSpectrumAnalysisOverlay();
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

void MainWindow::updateImageStatsForChannel(int channel, int width, int height, int pixfmt, const QByteArray& data)
{
    using namespace cli::proto;

    ChannelImageStats* stats = nullptr;
    if (channel == Raw16) {
        stats = &rawImageStats_;
    } else if (channel == SliceStitch16) {
        stats = &sliceImageStats_;
    }
    if (!stats) return;

    stats->valid = false;
    stats->min = 0;
    stats->max = 0;
    stats->avg = 0.0;

    if (pixfmt != Mono16) {
        refreshImageStatsOverlay();
        return;
    }

    Mono16Stats monoStats;
    if (!computeMono16Stats(data, width, height, &monoStats)) {
        refreshImageStatsOverlay();
        return;
    }

    const int pixels = width * height;
    stats->valid = true;
    stats->min = monoStats.min;
    stats->max = monoStats.max;
    stats->avg = pixels > 0 ? static_cast<double>(monoStats.sum) / pixels : 0.0;
    refreshImageStatsOverlay();
}

QString MainWindow::formatImageStatsText(const QPoint& cursorPos, const ChannelImageStats* stats) const
{
    const QString xText = cursorPos.x() >= 0 ? QString::number(cursorPos.x()) : QString::fromUtf8("--");
    const QString yText = cursorPos.y() >= 0 ? QString::number(cursorPos.y()) : QString::fromUtf8("--");

    QString minText = QString::fromUtf8("--");
    QString maxText = QString::fromUtf8("--");
    QString avgText = QString::fromUtf8("--");
    if (stats && stats->valid) {
        minText = QString::number(stats->min);
        maxText = QString::number(stats->max);
        avgText = QString::number(stats->avg, 'f', 1);
    }

    return QString::fromUtf8("x: %1  y: %2\nmin: %3  max: %4  avg: %5")
        .arg(xText, yText, minText, maxText, avgText);
}

void MainWindow::positionImageStatsOverlay()
{
    if (!imageStatsOverlay_ || !viewerContainer_) return;
    imageStatsOverlay_->adjustSize();
    const int margin = 12;
    const int x = qMax(margin, viewerContainer_->width() - imageStatsOverlay_->width() - margin);
    const int y = qMax(margin, viewerContainer_->height() - imageStatsOverlay_->height() - margin);
    imageStatsOverlay_->move(x, y);
    imageStatsOverlay_->raise();
}

void MainWindow::refreshImageStatsOverlay()
{
    if (!imageStatsOverlay_ || !imageStatsLabel_) return;

    const bool showOverlay = currentChannel_ == 0 || currentChannel_ == 1;
    if (!showOverlay) {
        imageStatsOverlay_->hide();
        return;
    }

    const QPoint cursorPos = cursorImagePos_[static_cast<std::size_t>(currentChannel_)];
    const ChannelImageStats* stats = nullptr;
    if (currentChannel_ == 0) {
        stats = &rawImageStats_;
    } else if (currentChannel_ == 1) {
        stats = &sliceImageStats_;
    }

    imageStatsLabel_->setText(formatImageStatsText(cursorPos, stats));
    imageStatsOverlay_->adjustSize();
    positionImageStatsOverlay();
    imageStatsOverlay_->show();
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
    spectrumAnalysisPanel_ = new SpectrumAnalysisPanel(this);

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
    panelStack_->addWidget(wrapInScroll(spectrumAnalysisPanel_));
    // Panel 10: Logs jump to bottom log panel.
}

void MainWindow::onPanelSelected(int index)
{
    if (index < 0 || index > kLogPanelIndex) return;

    // Logs toggle bottom log panel expansion.
    if (index == kLogPanelIndex) {
        logPanel_->toggleExpanded();
        return;
    }

    panelStack_->setCurrentIndex(index);
    panelTitle_->setText(index == kSpectrumAnalysisPanelIndex
                             ? QString::fromUtf8("光谱分析")
                             : QString::fromUtf8(kPanelNames[index]));
    panelTitle_->setCursor(index == kSpectrumAnalysisPanelIndex ? Qt::PointingHandCursor : Qt::ArrowCursor);

    if (index == kSpectrumAnalysisPanelIndex) {
        currentChannel_ = 1;
        viewerStack_->setCurrentIndex(1);
        updateChannelTabStyle(1);
        openSpectrumAnalysisDialog();
    } else {
        refreshSpectrumAnalysisOverlay();
    }

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
        if (frame.channel == SliceStitch16 && frame.pixfmt == Mono16) {
            latestSliceData_ = frame.data;
            latestSliceWidth_ = frame.width;
            latestSliceHeight_ = frame.height;
            latestSliceFrameId_ = frame.streamFrameId;
            if (spectrumAnalysisPanel_) {
                spectrumAnalysisPanel_->setSliceGeometry(frame.width, frame.height, true);
            }
            refreshSpectrumAnalysisOverlay();
        }
        if (frame.channel == Raw16 || frame.channel == SliceStitch16) {
            updateImageStatsForChannel(frame.channel, frame.width, frame.height, frame.pixfmt, frame.data);
        }

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
        refreshImageStatsOverlay();
        refreshSpectralProgressOverlay();
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
            clearSpectralProgress(SpectralSource::Playback);
            playbackRawSpectral_.reset();
            playbackSliceSpectral_.reset();
            refreshSpectralSource();
            refreshImageStatsOverlay();
        });
        connect(recordPlaybackPanel_->playback(), &FramePlaybackController::playbackStopped, this, [this]() {
            playbackActive_ = false;
            refreshSpectralSource();
            refreshSpectralStats();
            refreshImageStatsOverlay();
        });
    }

    connect(spectralPanel_, &SpectralPanel::settingsChanged, this, [this]() {
        refreshSpectralSource();
        refreshSpectralStats();
        refreshImageStatsOverlay();
        refreshSpectralProgressOverlay();
    });

    connect(spectrumAnalysisPanel_, &SpectrumAnalysisPanel::settingsChanged, this, [this]() {
        if (spectrumCurveDialog_) {
            spectrumCurveDialog_->setRefreshRateHz(spectrumAnalysisPanel_->refreshRateHz());
        }
        forceRefreshSpectrumCurves();
    });
    connect(spectrumAnalysisPanel_, &SpectrumAnalysisPanel::linesChanged, this, [this]() {
        refreshSpectrumAnalysisOverlay();
        forceRefreshSpectrumCurves();
    });
    connect(spectrumAnalysisPanel_, &SpectrumAnalysisPanel::showDialogRequested,
            this, &MainWindow::openSpectrumAnalysisDialog);

    connect(imageViewRaw_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[0] = pos;
        refreshImageStatsOverlay();
    });
    connect(imageViewSlice_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[1] = pos;
        refreshImageStatsOverlay();
    });
    connect(imageViewSpectral_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[2] = pos;
    });
    connect(imageViewPlayback_, &ImageView::cursorImagePosChanged, this, [this](const QPoint& pos) {
        cursorImagePos_[3] = pos;
    });
    connect(imageViewSlice_, &ImageView::analysisLineAddRequested, this, [this](int y) {
        if (spectrumAnalysisPanel_) spectrumAnalysisPanel_->addLineAt(y);
    });
    connect(imageViewSlice_, &ImageView::analysisLineMoveRequested, this, [this](int index, int y) {
        if (spectrumAnalysisPanel_) spectrumAnalysisPanel_->moveLine(index, y);
    });
    connect(imageViewSlice_, &ImageView::analysisLineDeleteRequested, this, [this](int index) {
        if (spectrumAnalysisPanel_) spectrumAnalysisPanel_->deleteLine(index);
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
    if (panelVersion < 3) {
        // Migration: v2 index 9 was logs; v3 inserts spectrum analysis at 9.
        if (panel == 9) panel = kLogPanelIndex;
    }

    if (panel >= 0 && panel <= kLogPanelIndex) {
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
    s.setValue("window/panelVersion", 3);
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
    refreshSpectralProgressOverlay();
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

void MainWindow::startSpectralProgress(SpectralSource source, int channel)
{
    const int idx = spectralProgressIndex(source, channel);
    if (idx < 0) return;

    auto& state = spectralProgressStates_[static_cast<std::size_t>(idx)];
    state.active = true;
    state.startedMs = uptime_.elapsed();
    refreshSpectralProgressOverlay();
}

void MainWindow::stopSpectralProgress(SpectralSource source, int channel)
{
    const int idx = spectralProgressIndex(source, channel);
    if (idx < 0) return;

    auto& state = spectralProgressStates_[static_cast<std::size_t>(idx)];
    state.active = false;
    state.startedMs = -1;
    refreshSpectralProgressOverlay();
}

void MainWindow::clearSpectralProgress(SpectralSource source)
{
    const int sourceIndex = source == SpectralSource::Live ? 0 : 1;
    for (int i = 0; i < 2; ++i) {
        auto& state = spectralProgressStates_[static_cast<std::size_t>(sourceIndex * 2 + i)];
        state.active = false;
        state.startedMs = -1;
    }
    refreshSpectralProgressOverlay();
}

void MainWindow::positionSpectralProgressOverlay()
{
    if (!spectralProgressOverlay_ || !viewerContainer_) return;
    spectralProgressOverlay_->adjustSize();
    const int margin = 12;
    int x = (viewerContainer_->width() - spectralProgressOverlay_->width()) / 2;
    if (x < margin) x = margin;
    if (x + spectralProgressOverlay_->width() + margin > viewerContainer_->width()) {
        x = qMax(margin, viewerContainer_->width() - spectralProgressOverlay_->width() - margin);
    }
    int y = viewerContainer_->height() - spectralProgressOverlay_->height() - margin;
    if (y < margin) y = margin;
    spectralProgressOverlay_->move(x, y);
    spectralProgressOverlay_->raise();
}

void MainWindow::refreshSpectralProgressOverlay()
{
    if (!spectralProgressOverlay_ || !spectralProgressLabel_ || !spectralProgressBar_ || !spectralPanel_) return;

    if (currentChannel_ != 2) {
        spectralProgressOverlay_->hide();
        return;
    }

    const int channel = spectralPanel_->sourceChannel();
    const SpectralProgressState* state = nullptr;
    const int idx = spectralProgressIndex(spectralSource_, channel);
    if (idx >= 0) {
        state = &spectralProgressStates_[static_cast<std::size_t>(idx)];
    }
    if (!state || !state->active || state->startedMs < 0) {
        spectralProgressOverlay_->hide();
        return;
    }

    const qint64 elapsedMs = qMax<qint64>(0, uptime_.elapsed() - state->startedMs);
    const int progress = elapsedMs >= 10000 ? 100 : static_cast<int>((elapsedMs * 100) / 10000);
    LOGD << "Spectral progress: " << progress << "% (elapsed " << elapsedMs << " ms)";
    spectralProgressLabel_->setText(QString::fromUtf8("等待整帧完整数据"));
    spectralProgressBar_->setValue(qBound(0, progress, 100));
    positionSpectralProgressOverlay();
    spectralProgressOverlay_->show();
}

bool MainWindow::isSpectrumAnalysisActive() const
{
    return sidebar_ && sidebar_->activePanel() == kSpectrumAnalysisPanelIndex;
}

void MainWindow::openSpectrumAnalysisDialog()
{
    if (!spectrumCurveDialog_) {
        spectrumCurveDialog_ = new SpectrumCurveDialog(this);
        if (spectrumAnalysisPanel_) {
            spectrumCurveDialog_->setRefreshRateHz(spectrumAnalysisPanel_->refreshRateHz());
        }
        connect(spectrumCurveDialog_, &SpectrumCurveDialog::sampleRefreshRequested,
                this, &MainWindow::updateSpectrumCurveData);
    }

    spectrumCurveDialog_->show();
    spectrumCurveDialog_->raise();
    spectrumCurveDialog_->activateWindow();
    refreshSpectrumAnalysisOverlay();
    forceRefreshSpectrumCurves();
}

void MainWindow::refreshSpectrumAnalysisOverlay()
{
    if (!imageViewSlice_) return;
    const bool enabled = isSpectrumAnalysisActive() && currentChannel_ == 1;
    imageViewSlice_->setAnalysisOverlayEnabled(enabled);
    imageViewSlice_->setAnalysisLines(spectrumAnalysisPanel_ ? spectrumAnalysisPanel_->lines()
                                                             : QVector<SpectrumSampleLine>());
}

void MainWindow::forceRefreshSpectrumCurves()
{
    updateSpectrumCurveData(true);
}

void MainWindow::updateSpectrumCurveData(bool force)
{
    if (!spectrumAnalysisPanel_ || !spectrumCurveDialog_) return;
    const double yRangeMultiplier = spectrumAnalysisPanel_->yRangeMultiplier();
    const double yMinPositionPercent = spectrumAnalysisPanel_->yMinPositionPercent();
    const double yMinDataSpan = spectrumAnalysisPanel_->yMinDataSpan();

    if (latestSliceData_.isEmpty() || latestSliceWidth_ <= 0 || latestSliceHeight_ <= 0) {
        spectrumAnalysisPanel_->setStatusText(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"));
        spectrumCurveDialog_->setStatusText(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"));
        spectrumCurveDialog_->setCurveData(QVector<QVector<double>>(),
                                           QVector<QVector<double>>(),
                                           QVector<QString>(),
                                           QVector<QColor>(),
                                           QString::fromUtf8("波长"),
                                           yRangeMultiplier,
                                           yMinPositionPercent,
                                           yMinDataSpan);
        return;
    }
    if (!force && lastCurveFrameId_ == latestSliceFrameId_) {
        return;
    }

    const QVector<SpectrumSampleLine> lines = spectrumAnalysisPanel_->lines();
    if (lines.isEmpty()) {
        spectrumCurveDialog_->setCurveData(QVector<QVector<double>>(),
                                           QVector<QVector<double>>(),
                                           QVector<QString>(),
                                           QVector<QColor>(),
                                           QString::fromUtf8("波长"),
                                           yRangeMultiplier,
                                           yMinPositionPercent,
                                           yMinDataSpan);
        spectrumAnalysisPanel_->setStatusText(QString::fromUtf8("点击 SliceStitch16 图像添加采样线"));
        lastCurveFrameId_ = latestSliceFrameId_;
        return;
    }

    const int x0 = qBound(0, spectrumAnalysisPanel_->xStart(), latestSliceWidth_ - 1);
    const int x1 = qBound(0, spectrumAnalysisPanel_->xEnd(), latestSliceWidth_ - 1);
    const int startX = qMin(x0, x1);
    const int endX = qMax(x0, x1);
    if (startX == endX || latestSliceData_.size() < latestSliceWidth_ * latestSliceHeight_ * 2) {
        spectrumCurveDialog_->setStatusText(QString::fromUtf8("映射范围或图像数据无效"));
        return;
    }

    const double waveStart = spectrumAnalysisPanel_->wavelengthStart();
    const double waveEnd = spectrumAnalysisPanel_->wavelengthEnd();
    const double denom = static_cast<double>(endX - startX);
    const int filterWindow = spectrumAnalysisPanel_->filterWindowPixels();
    const int maxPlotPoints = spectrumAnalysisPanel_->maxPlotPoints();
    const QVector<int> sampledXs = spectrumSampledXs(startX, endX, maxPlotPoints);
    const auto* pixels = reinterpret_cast<const quint16*>(latestSliceData_.constData());

    QVector<QVector<double>> xList;
    QVector<QVector<double>> yList;
    QVector<QString> names;
    QVector<QColor> colors;
    for (const SpectrumSampleLine& line : lines) {
        const int y = qBound(0, line.y, latestSliceHeight_ - 1);
        QVector<double> xs;
        QVector<double> ys;
        xs.reserve(sampledXs.size());
        ys.reserve(sampledXs.size());
        const int rowOffset = y * latestSliceWidth_;
        for (int x : sampledXs) {
            const double t = static_cast<double>(x - startX) / denom;
            xs.append(waveStart + t * (waveEnd - waveStart));
            ys.append(spectrumMovingAverage(pixels, rowOffset, latestSliceWidth_, x, filterWindow));
        }
        xList.append(xs);
        yList.append(ys);
        names.append(line.name());
        colors.append(line.color);
    }

    spectrumCurveDialog_->setCurveData(xList, yList, names, colors, QString::fromUtf8("波长"),
                                       yRangeMultiplier,
                                       yMinPositionPercent,
                                       yMinDataSpan);
    spectrumAnalysisPanel_->setStatusText(QString::fromUtf8("已更新 %1 条曲线，帧 %2")
                                              .arg(lines.size())
                                              .arg(latestSliceFrameId_));
    lastCurveFrameId_ = latestSliceFrameId_;
}

void MainWindow::handleLiveFrame(const StreamFrame& frame)
{
    using namespace cli::proto;
    if (frame.frameType != HeaderFrame && frame.frameType != DataFrame && frame.frameType != TailFrame) {
        return;
    }

    SpectralScanBuilder* b = builderFor(SpectralSource::Live, frame.channel);
    if (!b) return;
    const bool wasActive = b->hasActiveScan();
    const bool committed = b->feedFrame(frame.frameType, frame.streamFrameId, frame.width, frame.height, frame.pixfmt, frame.data);
    const bool hasActive = b->hasActiveScan();
    const bool spectralVisible = currentChannel_ == 2;
    const bool sourceMatches = (spectralSource_ == SpectralSource::Live);
    const bool channelMatches = sourceMatches && spectralPanel_ && spectralPanel_->sourceChannel() == frame.channel;
    if (frame.frameType == HeaderFrame) {
        if (hasActive) {
            startSpectralProgress(SpectralSource::Live, frame.channel);
        } else if (wasActive && !committed) {
            stopSpectralProgress(SpectralSource::Live, frame.channel);
        }
    } else if (frame.frameType == TailFrame) {
        if (committed) {
            stopSpectralProgress(SpectralSource::Live, frame.channel);
        } else if (wasActive && !hasActive) {
            stopSpectralProgress(SpectralSource::Live, frame.channel);
        }
    } else if (wasActive && !hasActive && !committed) {
        stopSpectralProgress(SpectralSource::Live, frame.channel);
    }
    if (spectralVisible && channelMatches) {
        refreshSpectralStats();
    }
    if (sourceMatches && committed) {
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
    const bool wasActive = b->hasActiveScan();
    const bool committed = b->feedFrame(frame.frameType, frame.streamFrameId, frame.width, frame.height, frame.pixfmt, frame.data);
    const bool hasActive = b->hasActiveScan();
    const bool spectralVisible = currentChannel_ == 2;
    const bool sourceMatches = (spectralSource_ == SpectralSource::Playback);
    const bool channelMatches = sourceMatches && spectralPanel_ && spectralPanel_->sourceChannel() == frame.channel;
    if (frame.frameType == HeaderFrame) {
        if (hasActive) {
            startSpectralProgress(SpectralSource::Playback, frame.channel);
        } else if (wasActive && !committed) {
            stopSpectralProgress(SpectralSource::Playback, frame.channel);
        }
    } else if (frame.frameType == TailFrame) {
        if (committed) {
            stopSpectralProgress(SpectralSource::Playback, frame.channel);
        } else if (wasActive && !hasActive) {
            stopSpectralProgress(SpectralSource::Playback, frame.channel);
        }
    } else if (wasActive && !hasActive && !committed) {
        stopSpectralProgress(SpectralSource::Playback, frame.channel);
    }
    if (spectralVisible && channelMatches) {
        refreshSpectralStats();
    }
    if (sourceMatches && committed) {
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
    if (!b || !b->hasRenderableData()) {
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

#include "DeviceUiCoordinator.h"

#include "DeviceClient.h"
#include "ImageFrameUtils.h"
#include "MainWindowChrome.h"
#include "MainWindowPanelRegistry.h"
#include "SpectrumAnalysisCoordinator.h"
#include "Client/stream/StreamFrame.h"
#include "Protocol.h"
#include "panels/ConnectionPanel.h"
#include "panels/DashboardPanel.h"
#include "panels/LogPanel.h"
#include "panels/RecordPlaybackPanel.h"
#include "panels/SpectralPanel.h"
#include "panels/StreamPanel.h"
#include "widgets/SidebarWidget.h"
#include "widgets/TopBarWidget.h"
#include "widgets/ViewerAreaWidget.h"

#include <QImage>
#include <QTimer>

namespace {
int viewerChannelForStreamChannel(int channel)
{
    using namespace cli::proto;

    if (channel == Raw16) return ViewerAreaWidget::Raw16View;
    if (channel == SliceStitch16) return ViewerAreaWidget::SliceStitch16View;
    return -1;
}
} // namespace

DeviceUiCoordinator::DeviceUiCoordinator(DeviceClient* device,
                                         MainWindowChrome* chrome,
                                         MainWindowPanelRegistry* registry,
                                         SpectrumAnalysisCoordinator* spectrumAnalysisCoordinator,
                                         QObject* parent)
    : QObject(parent)
    , device_(device)
    , chrome_(chrome)
    , registry_(registry)
    , spectrumAnalysisCoordinator_(spectrumAnalysisCoordinator)
{
    setupConnections();
}

void DeviceUiCoordinator::startTimersAndRefresh()
{
    uptime_.start();

    uptimeTimer_ = new QTimer(this);
    connect(uptimeTimer_, &QTimer::timeout, this, [this]() {
        qint64 secs = uptime_.elapsed() / 1000;
        int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
        registry_->dashboard()->setUptime(QString("%1:%2:%3")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0')));
    });
    uptimeTimer_->start(1000);

    spectralRenderTimer_ = new QTimer(this);
    spectralRenderTimer_->setInterval(33);
    connect(spectralRenderTimer_, &QTimer::timeout, this, [this]() {
        if (!spectralDirty_ ||
            chrome_->viewerArea()->currentChannel() != ViewerAreaWidget::SpectralView) {
            return;
        }
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
    refreshSpectralProgressOverlay();
    refreshStreamStats();
    refreshConnectionDashboard();
}

void DeviceUiCoordinator::refreshStreamStats()
{
    if (!device_ || !device_->stream() || !chrome_ || !registry_) return;

    using namespace cli::proto;
    int fpsChannel = 0;
    const int currentChannel = chrome_->viewerArea()->currentChannel();
    if (currentChannel == ViewerAreaWidget::Raw16View) {
        fpsChannel = Raw16;
    } else if (currentChannel == ViewerAreaWidget::SliceStitch16View) {
        fpsChannel = SliceStitch16;
    } else if (currentChannel == ViewerAreaWidget::SpectralView) {
        fpsChannel = registry_->spectral()->sourceChannel();
    }

    const double selectedFps = fpsChannel > 0 ? device_->stream()->fps(fpsChannel) : device_->stream()->fps();
    chrome_->topBar()->setFps(selectedFps);
    chrome_->topBar()->setFrames(device_->stream()->framesReceived());
    chrome_->topBar()->setDropped(device_->stream()->framesDropped());

    registry_->dashboard()->setStreamStats(
        device_->stream()->fps(Raw16),
        device_->stream()->fps(SliceStitch16),
        device_->stream()->framesReceived(),
        device_->stream()->framesDropped());
}

void DeviceUiCoordinator::refreshConnectionDashboard()
{
    registry_->dashboard()->setConnectionInfo(hostIp_, tcpPort_, udpPort_,
                                              device_->isConnected(), deviceVersion_);
}

void DeviceUiCoordinator::stopPlaybackForClose()
{
    auto* panel = registry_->recordPlayback();
    if (panel) {
        panel->cancelRemoteWork();
    }
}

void DeviceUiCoordinator::disconnectDevice()
{
    if (device_) {
        device_->disconnect();
    }
}

void DeviceUiCoordinator::setupConnections()
{
    connect(chrome_->viewerArea(), &ViewerAreaWidget::channelChanged, this, [this](int channel) {
        if (channel == ViewerAreaWidget::SpectralView) {
            refreshSpectralSource();
            refreshSpectralStats();
        }
        refreshStreamStats();
        refreshSpectralProgressOverlay();
    });

    connect(chrome_->sidebar(), &SidebarWidget::panelSelected, this, [this](int index) {
        if (index == MainWindowPanelRegistry::Dashboard) {
            refreshConnectionDashboard();
        }
    });

    connect(device_, &DeviceClient::connectionChanged, this,
            [this](bool connected, const QString& ip) {
        hostIp_ = connected ? ip : QString();
        chrome_->sidebar()->setConnected(connected);
        chrome_->topBar()->setConnected(connected, ip);

        if (connected) {
            quint16 port = registry_->connection()->udpPort();
            udpPort_ = port;
            registry_->stream()->setUdpPort(port);
            if (!device_->stream()->isBound()) {
                device_->stream()->bind(port);
            }

            device_->systemApi()->version(this, [this](bool ok, int ver, const QString& name, const QString&) {
                if (ok) {
                    deviceVersion_ = QString("%1 v%2").arg(name).arg(ver);
                    refreshConnectionDashboard();
                }
            });
        }

        refreshConnectionDashboard();
    });

    connect(device_, &DeviceClient::mirrorAngleEvent, this,
            [this](double angle, bool moving, qint64) {
        chrome_->topBar()->setMirrorAngle(angle);
        registry_->dashboard()->setMirrorInfo(angle, moving);
    });

    connect(device_, &DeviceClient::frameReady, this,
            [this](const StreamFrame& frame) {
        using namespace cli::proto;

        handleLiveFrame(frame);

        if (frame.channel == SliceStitch16 && frame.pixfmt == Mono16 && spectrumAnalysisCoordinator_) {
            spectrumAnalysisCoordinator_->setLatestSliceFrame(frame.width, frame.height,
                                                              frame.streamFrameId, frame.data);
        }

        const int viewerChannel = viewerChannelForStreamChannel(frame.channel);
        if (viewerChannel >= 0) {
            chrome_->viewerArea()->setImageStats(
                viewerChannel,
                makeChannelImageStats(frame.width, frame.height, frame.pixfmt, frame.data));
            chrome_->viewerArea()->renderFrame(viewerChannel, frame.width, frame.height, frame.pixfmt, frame.data);
        }

        refreshStreamStats();
    }, Qt::QueuedConnection);

    connect(device_->stream(), &StreamClient::statsUpdated, this, &DeviceUiCoordinator::refreshStreamStats);

    connect(device_->control(), &ControlClient::rawLog, this,
            [this](const QString& line) {
        chrome_->logPanel()->appendLog(line);
    });

    connect(registry_->stream(), &StreamPanel::subscribeRequested, this,
            [this](quint16 port, const QStringList&) {
        if (!device_->stream()->isBound()) {
            device_->stream()->bind(port);
        }
    });

    connect(registry_->recordPlayback(), &RecordPlaybackPanel::requestSwitchToPlaybackView, this, [this]() {
        chrome_->viewerArea()->setCurrentChannel(ViewerAreaWidget::PlaybackView);
        refreshSpectralProgressOverlay();
    });

    connect(registry_->recordPlayback(), &RecordPlaybackPanel::playbackImageReady, this,
            [this](const QImage& image, const QString&, const ChannelImageStats& stats) {
        chrome_->viewerArea()->setCurrentChannel(ViewerAreaWidget::PlaybackView);
        chrome_->viewerArea()->setChannelImage(ViewerAreaWidget::PlaybackView, image);
        chrome_->viewerArea()->setImageStats(ViewerAreaWidget::PlaybackView, stats);
    }, Qt::QueuedConnection);

    connect(registry_->spectral(), &SpectralPanel::settingsChanged, this, [this]() {
        refreshSpectralSource();
        refreshSpectralStats();
        refreshSpectralProgressOverlay();
    });
}

void DeviceUiCoordinator::refreshSpectralSource()
{
    spectralController_.setSource(
        spectralController_.effectiveSource(registry_->spectral()->sourceMode()));
    spectralDirty_ = true;
    refreshSpectralProgressOverlay();
}

void DeviceUiCoordinator::refreshSpectralStats()
{
    const SpectralSource source = spectralController_.source();
    const int channel = registry_->spectral()->sourceChannel();
    const SpectralStats stats = spectralController_.stats(source, channel);
    const QString activeSource = source == SpectralSource::Live ? "Live" : "Playback";

    registry_->spectral()->setBandCount(stats.bands);
    registry_->spectral()->setStats(activeSource, stats.scanWidth, stats.height, stats.bands,
                                    stats.tailSeen, stats.active, stats.gapFillColumns);
}

void DeviceUiCoordinator::refreshSpectralProgressOverlay()
{
    if (chrome_->viewerArea()->currentChannel() != ViewerAreaWidget::SpectralView) {
        chrome_->viewerArea()->setSpectralProgressVisible(false, 0, QString());
        return;
    }

    const SpectralProgress progress = spectralController_.progress(
        spectralController_.source(), registry_->spectral()->sourceChannel(), uptime_.elapsed());
    chrome_->viewerArea()->setSpectralProgressVisible(progress.active,
                                                      progress.percent,
                                                      QString::fromUtf8("等待整帧完整数据"));
}

void DeviceUiCoordinator::handleLiveFrame(const StreamFrame& frame)
{
    const SpectralFrameResult result = spectralController_.feedLiveFrame(frame, uptime_.elapsed());
    const bool sourceMatches = spectralController_.source() == SpectralSource::Live;
    const bool channelMatches = sourceMatches && registry_->spectral()->sourceChannel() == frame.channel;

    if (chrome_->viewerArea()->currentChannel() == ViewerAreaWidget::SpectralView && channelMatches) {
        refreshSpectralStats();
    }
    if (sourceMatches && result.committed) {
        spectralDirty_ = true;
    }
    if (result.progressChanged) {
        refreshSpectralProgressOverlay();
    }
}

void DeviceUiCoordinator::updateSpectralView()
{
    const int channel = registry_->spectral()->sourceChannel();
    refreshSpectralStats();

    const QImage img = spectralController_.render(
        spectralController_.source(), channel, registry_->spectral()->renderOptions());
    if (img.isNull()) {
        chrome_->viewerArea()->setSpectralNoSignal();
        return;
    }

    chrome_->viewerArea()->setSpectralImage(img);
}

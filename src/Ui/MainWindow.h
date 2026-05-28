#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QTimer>
#include <array>

#include "Client/stream/StreamFrame.h"
#include "SpectralScanBuilder.h"
#include "SpectrumAnalysisTypes.h"

class DeviceClient;
class SidebarWidget;
class TopBarWidget;
class DashboardPanel;
class ConnectionPanel;
class CameraPanel;
class MirrorPanel;
class IrPanel;
class CollectPanel;
class StreamPanel;
class LogPanel;
class ImageView;
class RecordPlaybackPanel;
class SpectralPanel;
class SpectrumAnalysisPanel;
class SpectrumCurveDialog;
struct RecordedFrame;

enum class SpectralSource {
    Live = 0,
    Playback = 1,
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    struct ChannelImageStats {
        bool valid = false;
        quint16 min = 0;
        quint16 max = 0;
        double avg = 0.0;
    };

    void setupUi();
    void setupPanels();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void onPanelSelected(int index);
    void updateChannelTabStyle(int tab);
    void renderFrameToView(ImageView* target, int width, int height, int pixfmt, const QByteArray& data);
    void updateImageStatsForChannel(int channel, int width, int height, int pixfmt, const QByteArray& data);
    void refreshImageStatsOverlay();
    void positionImageStatsOverlay();
    QString formatImageStatsText(const QPoint& cursorPos, const ChannelImageStats* stats) const;
    void refreshSpectralSource();
    SpectralSource effectiveSpectralSource() const;
    void handleLiveFrame(const StreamFrame& frame);
    void handlePlaybackFrame(const RecordedFrame& frame);
    void refreshStreamStats();
    void updateSpectralView();
    SpectralScanBuilder* builderFor(SpectralSource source, int channel);
    const SpectralScanBuilder* builderFor(SpectralSource source, int channel) const;
    void refreshSpectralStats();
    void startSpectralProgress(SpectralSource source, int channel);
    void stopSpectralProgress(SpectralSource source, int channel);
    void clearSpectralProgress(SpectralSource source);
    void refreshSpectralProgressOverlay();
    void positionSpectralProgressOverlay();
    void openSpectrumAnalysisDialog();
    void refreshSpectrumAnalysisOverlay();
    void forceRefreshSpectrumCurves();
    void updateSpectrumCurveData(bool force);
    bool isSpectrumAnalysisActive() const;

    DeviceClient* device_ = nullptr;

    // New layout components
    SidebarWidget*  sidebar_     = nullptr;
    TopBarWidget*   topBar_      = nullptr;
    QSplitter*      mainSplitter_ = nullptr;

    // Viewer area
    QWidget*        viewerContainer_ = nullptr;
    QWidget*        channelTabBar_   = nullptr;
    QPushButton*    chTabRaw16_      = nullptr;
    QPushButton*    chTabSlice_      = nullptr;
    QPushButton*    chTabSpectral_   = nullptr;
    QPushButton*    chTabPlayback_   = nullptr;
    QStackedWidget* viewerStack_     = nullptr;
    ImageView*      imageViewRaw_    = nullptr;
    ImageView*      imageViewSlice_  = nullptr;
    ImageView*      imageViewSpectral_ = nullptr;
    ImageView*      imageViewPlayback_ = nullptr;

    // Viewer overlays
    QWidget*        zoomBar_       = nullptr;
    QWidget*        imageStatsOverlay_ = nullptr;
    QLabel*         imageStatsLabel_ = nullptr;
    QWidget*        spectralProgressOverlay_ = nullptr;
    QLabel*         spectralProgressLabel_ = nullptr;
    QProgressBar*   spectralProgressBar_ = nullptr;
    int             currentChannel_ = 0;
    std::array<QPoint, 4> cursorImagePos_ = {
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
        QPoint(-1, -1),
    };
    ChannelImageStats rawImageStats_;
    ChannelImageStats sliceImageStats_;

    // Right panel
    QWidget*        rightPanel_    = nullptr;
    QLabel*         panelTitle_    = nullptr;
    QStackedWidget* panelStack_    = nullptr;

    // Panels
    DashboardPanel*  dashPanel_     = nullptr;
    ConnectionPanel* connPanel_     = nullptr;
    CameraPanel*     cameraPanel_   = nullptr;
    MirrorPanel*     mirrorPanel_   = nullptr;
    IrPanel*         irPanel_       = nullptr;
    CollectPanel*    collectPanel_  = nullptr;
    StreamPanel*     streamPanel_   = nullptr;
    SpectralPanel*   spectralPanel_ = nullptr;
    RecordPlaybackPanel* recordPlaybackPanel_ = nullptr;
    SpectrumAnalysisPanel* spectrumAnalysisPanel_ = nullptr;
    SpectrumCurveDialog* spectrumCurveDialog_ = nullptr;

    // Bottom log
    LogPanel*       logPanel_      = nullptr;

    // metadata
    QString hostIp_;
    quint16 tcpPort_ = 9000;
    quint16 udpPort_ = 1400;
    QString deviceVersion_;
    QElapsedTimer uptime_;
    QByteArray latestSliceData_;
    int latestSliceWidth_ = 0;
    int latestSliceHeight_ = 0;
    quint64 latestSliceFrameId_ = 0;
    quint64 lastCurveFrameId_ = 0;

    SpectralScanBuilder liveRawSpectral_;
    SpectralScanBuilder liveSliceSpectral_;
    SpectralScanBuilder playbackRawSpectral_;
    SpectralScanBuilder playbackSliceSpectral_;
    SpectralSource spectralSource_ = SpectralSource::Live;
    bool playbackActive_ = false;
    QTimer* spectralRenderTimer_ = nullptr;
    QTimer* spectralProgressTimer_ = nullptr;
    bool spectralDirty_ = false;

    struct SpectralProgressState {
        bool active = false;
        qint64 startedMs = -1;
    };
    std::array<SpectralProgressState, 4> spectralProgressStates_ = {};
};

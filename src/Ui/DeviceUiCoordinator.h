#pragma once

#include <QObject>
#include <QElapsedTimer>

#include "SpectralScanController.h"

class DeviceClient;
class MainWindowChrome;
class MainWindowPanelRegistry;
class SpectrumAnalysisCoordinator;
class QWidget;
struct RecordedFrame;
struct StreamFrame;

class DeviceUiCoordinator : public QObject {
    Q_OBJECT
public:
    DeviceUiCoordinator(DeviceClient* device,
                        MainWindowChrome* chrome,
                        MainWindowPanelRegistry* registry,
                        SpectrumAnalysisCoordinator* spectrumAnalysisCoordinator,
                        QObject* parent = nullptr);

    void startTimersAndRefresh();
    void refreshStreamStats();
    void refreshConnectionDashboard();
    bool stopRecordingForClose(QWidget* parent);
    void stopPlaybackForClose();
    void disconnectDevice();

private:
    void setupConnections();
    void refreshSpectralSource();
    void refreshSpectralStats();
    void refreshSpectralProgressOverlay();
    void handleLiveFrame(const StreamFrame& frame);
    void handlePlaybackFrame(const RecordedFrame& frame);
    void updateSpectralView();

    DeviceClient* device_ = nullptr;
    MainWindowChrome* chrome_ = nullptr;
    MainWindowPanelRegistry* registry_ = nullptr;
    SpectrumAnalysisCoordinator* spectrumAnalysisCoordinator_ = nullptr;
    SpectralScanController spectralController_;
    QElapsedTimer uptime_;
    QTimer* uptimeTimer_ = nullptr;
    QTimer* spectralRenderTimer_ = nullptr;
    QTimer* spectralProgressTimer_ = nullptr;
    QString hostIp_;
    quint16 tcpPort_ = 9000;
    quint16 udpPort_ = 1400;
    QString deviceVersion_;
    bool spectralDirty_ = false;
};

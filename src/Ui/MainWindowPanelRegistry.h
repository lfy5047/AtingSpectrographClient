#pragma once

#include <QObject>

class CameraPanel;
class CollectPanel;
class ConnectionPanel;
class DashboardPanel;
class DeviceClient;
class IrPanel;
class MainWindowChrome;
class MirrorPanel;
class RecordPlaybackPanel;
class SpectralPanel;
class SpectrumAnalysisCoordinator;
class SpectrumAnalysisPanel;
class StreamPanel;

class MainWindowPanelRegistry : public QObject {
    Q_OBJECT
public:
    enum PanelIndex {
        Dashboard = 0,
        Connection = 1,
        Camera = 2,
        Mirror = 3,
        Ir = 4,
        Collect = 5,
        Stream = 6,
        Spectral = 7,
        RecordPlayback = 8,
        SpectrumAnalysis = 9,
        Log = 10,
    };
    static const int PanelVersion = 3;

    MainWindowPanelRegistry(DeviceClient* device, MainWindowChrome* chrome, QObject* parent = nullptr);

    void setSpectrumAnalysisCoordinator(SpectrumAnalysisCoordinator* coordinator);
    void selectPanel(int index);
    int currentPanel() const { return currentPanel_; }

    DashboardPanel* dashboard() const { return dashPanel_; }
    ConnectionPanel* connection() const { return connPanel_; }
    StreamPanel* stream() const { return streamPanel_; }
    SpectralPanel* spectral() const { return spectralPanel_; }
    RecordPlaybackPanel* recordPlayback() const { return recordPlaybackPanel_; }
    SpectrumAnalysisPanel* spectrumAnalysis() const { return spectrumAnalysisPanel_; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupPanels();

    DeviceClient* device_ = nullptr;
    MainWindowChrome* chrome_ = nullptr;
    SpectrumAnalysisCoordinator* spectrumAnalysisCoordinator_ = nullptr;
    int currentPanel_ = Dashboard;

    DashboardPanel* dashPanel_ = nullptr;
    ConnectionPanel* connPanel_ = nullptr;
    CameraPanel* cameraPanel_ = nullptr;
    MirrorPanel* mirrorPanel_ = nullptr;
    IrPanel* irPanel_ = nullptr;
    CollectPanel* collectPanel_ = nullptr;
    StreamPanel* streamPanel_ = nullptr;
    SpectralPanel* spectralPanel_ = nullptr;
    RecordPlaybackPanel* recordPlaybackPanel_ = nullptr;
    SpectrumAnalysisPanel* spectrumAnalysisPanel_ = nullptr;
};

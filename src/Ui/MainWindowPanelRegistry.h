#pragma once

#include <QObject>

class AdvancedSettingsPanel;
class CalibrationPanel;
class CameraPanel;
class CollectPanel;
class ConnectionPanel;
class DashboardPanel;
class DataAcquisitionPanel;
class DeviceClient;
class IrPanel;
class MainWindowChrome;
class MirrorPanel;
class RecordPlaybackPanel;
class SpectralPanel;
class SpectralSegmentTestPanel;
class SpectrumAnalysisCoordinator;
class SpectrumAnalysisPanel;
class StreamPanel;
class TempControlPanel;

class MainWindowPanelRegistry : public QObject {
    Q_OBJECT
public:
    enum PanelIndex {
        Dashboard = 0,
        DataAcquisition = 1,
        DetectorSettings = 2,
        Calibration = 3,
        TempControl = 4,
        RecordPlayback = 5,
        SpectrumAnalysis = 6,
        AdvancedSettings = 7,
        SpectralSegmentTest = 8,
        Log = 9,
    };
    static const int PanelVersion = 6;

    MainWindowPanelRegistry(DeviceClient* device, MainWindowChrome* chrome, QObject* parent = nullptr);

    void setSpectrumAnalysisCoordinator(SpectrumAnalysisCoordinator* coordinator);
    void selectPanel(int index);
    int currentPanel() const { return currentPanel_; }

    DashboardPanel* dashboard() const { return dashPanel_; }
    DataAcquisitionPanel* dataAcquisition() const { return dataAcquisitionPanel_; }
    IrPanel* detectorSettings() const { return detectorPanel_; }
    CalibrationPanel* calibration() const { return calibrationPanel_; }
    TempControlPanel* tempControl() const { return tempControlPanel_; }
    RecordPlaybackPanel* recordPlayback() const { return recordPlaybackPanel_; }
    SpectrumAnalysisPanel* spectrumAnalysis() const { return spectrumAnalysisPanel_; }
    AdvancedSettingsPanel* advancedSettings() const { return advancedSettingsPanel_; }
    SpectralSegmentTestPanel* spectralSegmentTest() const { return spectralSegmentTestPanel_; }

    ConnectionPanel* connection() const;
    CameraPanel* camera() const;
    CollectPanel* collect() const;
    MirrorPanel* mirror() const;
    StreamPanel* stream() const;
    SpectralPanel* spectral() const;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupPanels();
    int preferredStreamViewerChannel() const;
    void selectAssociatedViewerChannel(int panelIndex);

    DeviceClient* device_ = nullptr;
    MainWindowChrome* chrome_ = nullptr;
    SpectrumAnalysisCoordinator* spectrumAnalysisCoordinator_ = nullptr;
    int currentPanel_ = Dashboard;

    DashboardPanel* dashPanel_ = nullptr;
    DataAcquisitionPanel* dataAcquisitionPanel_ = nullptr;
    IrPanel* detectorPanel_ = nullptr;
    CalibrationPanel* calibrationPanel_ = nullptr;
    TempControlPanel* tempControlPanel_ = nullptr;
    RecordPlaybackPanel* recordPlaybackPanel_ = nullptr;
    SpectrumAnalysisPanel* spectrumAnalysisPanel_ = nullptr;
    AdvancedSettingsPanel* advancedSettingsPanel_ = nullptr;
    SpectralSegmentTestPanel* spectralSegmentTestPanel_ = nullptr;
};

#pragma once

#include <QMainWindow>

class DeviceClient;
class BinningTestController;
class DeviceUiCoordinator;
class MainWindowChrome;
class MainWindowPanelRegistry;
class SpectrumAnalysisCoordinator;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    DeviceClient* device_ = nullptr;
    BinningTestController* binningTestController_ = nullptr;
    MainWindowChrome* chrome_ = nullptr;
    MainWindowPanelRegistry* panelRegistry_ = nullptr;
    SpectrumAnalysisCoordinator* spectrumAnalysisCoordinator_ = nullptr;
    DeviceUiCoordinator* deviceUiCoordinator_ = nullptr;
};

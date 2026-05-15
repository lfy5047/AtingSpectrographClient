#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QLabel>
#include <QTabWidget>

class DeviceClient;
class ConnectionPanel;
class CameraPanel;
class MirrorPanel;
class IrPanel;
class CollectPanel;
class StreamPanel;
class LogPanel;
class ImageView;
class StatusBarPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void setupUi();
    void setupSidebar();
    void setupCentral();
    void setupPanels();
    void setupStatusBar();
    void loadSettings();
    void saveSettings();

    DeviceClient* device_ = nullptr;

    QSplitter*      mainSplitter_ = nullptr;
    QListWidget*    sidebar_      = nullptr;
    QStackedWidget* panelStack_   = nullptr;
    QTabWidget*     imageTabs_    = nullptr;

    ConnectionPanel* connPanel_    = nullptr;
    CameraPanel*     cameraPanel_  = nullptr;
    MirrorPanel*     mirrorPanel_  = nullptr;
    IrPanel*         irPanel_      = nullptr;
    CollectPanel*    collectPanel_ = nullptr;
    StreamPanel*     streamPanel_  = nullptr;
    LogPanel*        logPanel_     = nullptr;

    ImageView*      imageViews_[4] = {};
    StatusBarPanel* statusPanel_   = nullptr;
};

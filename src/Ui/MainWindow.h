#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QElapsedTimer>

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

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();
    void setupPanels();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void onPanelSelected(int index);
    void updateChannelTabStyle(int tab);
    void renderFrameToView(ImageView* target, int width, int height, int pixfmt, const QByteArray& data);

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
    QPushButton*    chTabPlayback_   = nullptr;
    QStackedWidget* viewerStack_     = nullptr;
    ImageView*      imageViewRaw_    = nullptr;
    ImageView*      imageViewSlice_  = nullptr;
    ImageView*      imageViewPlayback_ = nullptr;

    // Zoom overlay
    QWidget*        zoomBar_       = nullptr;
    QLabel*         imgInfoLabel_  = nullptr;
    int             currentChannel_ = 0;

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
    RecordPlaybackPanel* recordPlaybackPanel_ = nullptr;

    // Bottom log
    LogPanel*       logPanel_      = nullptr;

    // metadata
    QString hostIp_;
    quint16 tcpPort_ = 9000;
    quint16 udpPort_ = 1400;
    QString deviceVersion_;
    QElapsedTimer uptime_;
};

#pragma once

#include <QObject>

class QLabel;
class LogPanel;
class MainWindow;
class QSplitter;
class QStackedWidget;
class SidebarWidget;
class TopBarWidget;
class ViewerAreaWidget;

class MainWindowChrome : public QObject {
    Q_OBJECT
public:
    explicit MainWindowChrome(MainWindow* window);

    SidebarWidget* sidebar() const { return sidebar_; }
    TopBarWidget* topBar() const { return topBar_; }
    QSplitter* mainSplitter() const { return mainSplitter_; }
    ViewerAreaWidget* viewerArea() const { return viewerArea_; }
    QLabel* panelTitle() const { return panelTitle_; }
    QStackedWidget* panelStack() const { return panelStack_; }
    LogPanel* logPanel() const { return logPanel_; }

private:
    void setupUi();

    MainWindow* window_ = nullptr;
    SidebarWidget* sidebar_ = nullptr;
    TopBarWidget* topBar_ = nullptr;
    QSplitter* mainSplitter_ = nullptr;
    ViewerAreaWidget* viewerArea_ = nullptr;
    QWidget* rightPanel_ = nullptr;
    QLabel* panelTitle_ = nullptr;
    QStackedWidget* panelStack_ = nullptr;
    LogPanel* logPanel_ = nullptr;
};

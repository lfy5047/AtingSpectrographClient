#include "MainWindowChrome.h"

#include "MainWindow.h"
#include "panels/LogPanel.h"
#include "widgets/SidebarWidget.h"
#include "widgets/TopBarWidget.h"
#include "widgets/ViewerAreaWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

MainWindowChrome::MainWindowChrome(MainWindow* window)
    : QObject(window)
    , window_(window)
{
    setupUi();
}

void MainWindowChrome::setupUi()
{
    auto* central = new QWidget(window_);
    central->setObjectName("mainCentral");
    window_->setCentralWidget(central);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    sidebar_ = new SidebarWidget(window_);
    root->addWidget(sidebar_);

    auto* rightSideWidget = new QWidget(window_);
    rightSideWidget->setObjectName("mainContent");
    auto* rightSide = new QVBoxLayout(rightSideWidget);
    rightSide->setContentsMargins(0, 0, 0, 0);
    rightSide->setSpacing(0);

    topBar_ = new TopBarWidget(window_);
    rightSide->addWidget(topBar_);

    mainSplitter_ = new QSplitter(Qt::Horizontal, window_);
    viewerArea_ = new ViewerAreaWidget(window_);
    mainSplitter_->addWidget(viewerArea_);

    rightPanel_ = new QWidget(window_);
    rightPanel_->setObjectName("panelContainer");
    rightPanel_->setMinimumWidth(520);
    rightPanel_->setMaximumWidth(720);
    auto* panelLayout = new QVBoxLayout(rightPanel_);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    panelTitle_ = new QLabel(QString::fromUtf8("仪表盘"), rightPanel_);
    panelTitle_->setObjectName("panelTitle");
    panelTitle_->setFixedHeight(46);
    panelLayout->addWidget(panelTitle_);

    panelStack_ = new QStackedWidget(rightPanel_);
    panelStack_->setObjectName("panelStack");
    panelLayout->addWidget(panelStack_, 1);

    mainSplitter_->addWidget(rightPanel_);
    mainSplitter_->setStretchFactor(0, 3);
    mainSplitter_->setStretchFactor(1, 1);
    rightSide->addWidget(mainSplitter_, 1);

    logPanel_ = new LogPanel(window_);
    logPanel_->setFixedHeight(36);
    rightSide->addWidget(logPanel_);

    root->addWidget(rightSideWidget, 1);
}

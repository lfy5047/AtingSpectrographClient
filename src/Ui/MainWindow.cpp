#include "MainWindow.h"

#include "AppVersion.h"
#include "BinningTestController.h"
#include "DeviceClient.h"
#include "DeviceUiCoordinator.h"
#include "MainWindowChrome.h"
#include "MainWindowPanelRegistry.h"
#include "RoiTestController.h"
#include "SpectrumAnalysisCoordinator.h"
#include "ThemeManager.h"
#include "WindowSettingsStore.h"
#include "widgets/SidebarWidget.h"
#include "widgets/TopBarWidget.h"
#include "widgets/ViewerAreaWidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(AppVersion::windowTitle());
    resize(1480, 880);
    setMinimumSize(1000, 600);

    device_ = new DeviceClient(this);
    chrome_ = new MainWindowChrome(this);
    panelRegistry_ = new MainWindowPanelRegistry(device_, chrome_, this);
    spectrumAnalysisCoordinator_ = new SpectrumAnalysisCoordinator(
        panelRegistry_->spectrumAnalysis(), chrome_->viewerArea(), this, this);
    panelRegistry_->setSpectrumAnalysisCoordinator(spectrumAnalysisCoordinator_);
    binningTestController_ = new BinningTestController(
        device_, panelRegistry_->binningTest(), chrome_->viewerArea()->binningCompareWidget(), this);
    roiTestController_ = new RoiTestController(
        device_, panelRegistry_->roiTest(), chrome_->viewerArea()->roiCompareWidget(), this);
    deviceUiCoordinator_ = new DeviceUiCoordinator(
        device_, chrome_, panelRegistry_, spectrumAnalysisCoordinator_, this);

    statusBar()->hide();

    connect(chrome_->sidebar(), &SidebarWidget::panelSelected,
            panelRegistry_, &MainWindowPanelRegistry::selectPanel);
    chrome_->topBar()->setTheme(ThemeManager::loadSavedTheme());
    connect(chrome_->topBar(), &TopBarWidget::themeChanged,
            this, [](ThemeManager::Theme theme) {
                if (qApp) ThemeManager::applyTheme(*qApp, theme);
            });

    const int restoredPanel = WindowSettingsStore::restore(
        this, chrome_->mainSplitter(), chrome_->sidebar());
    chrome_->sidebar()->setActivePanel(restoredPanel);
    panelRegistry_->selectPanel(restoredPanel);

    deviceUiCoordinator_->startTimersAndRefresh();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* e)
{
    const bool roiReady = roiTestController_->prepareForClose();
    const bool binningReady = binningTestController_->prepareForClose();
    if (!roiReady) {
        QMessageBox::information(this, QString::fromUtf8("开窗测试"),
                                 roiTestController_->closeBlockReason());
        e->ignore();
        return;
    }
    if (!binningReady) {
        QMessageBox::information(this, QString::fromUtf8("Binning 测试"),
                                 binningTestController_->closeBlockReason());
        e->ignore();
        return;
    }
    deviceUiCoordinator_->stopPlaybackForClose();
    WindowSettingsStore::save(this,
                              chrome_->mainSplitter(),
                              chrome_->sidebar(),
                              panelRegistry_->currentPanel());
    deviceUiCoordinator_->disconnectDevice();
    QMainWindow::closeEvent(e);
}

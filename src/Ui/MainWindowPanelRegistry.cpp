#include "MainWindowPanelRegistry.h"

#include "DeviceClient.h"
#include "MainWindowChrome.h"
#include "SpectrumAnalysisCoordinator.h"
#include "panels/CameraPanel.h"
#include "panels/CollectPanel.h"
#include "panels/ConnectionPanel.h"
#include "panels/DashboardPanel.h"
#include "panels/IrPanel.h"
#include "panels/LogPanel.h"
#include "panels/MirrorPanel.h"
#include "panels/RecordPlaybackPanel.h"
#include "panels/SpectralPanel.h"
#include "panels/SpectrumAnalysisPanel.h"
#include "panels/StreamPanel.h"
#include "widgets/ViewerAreaWidget.h"

#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>

namespace {
const char* kPanelNames[] = {
    "仪表盘",
    "设备连接",
    "相机设置",
    "转镜控制",
    "红外热像",
    "数据采集",
    "流通道",
    "光谱显示",
    "录制回放",
    "系统日志",
};

QScrollArea* wrapInScroll(QWidget* w)
{
    auto* area = new QScrollArea;
    area->setObjectName("panelScroll");
    area->setWidgetResizable(true);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area->setWidget(w);
    return area;
}
} // namespace

MainWindowPanelRegistry::MainWindowPanelRegistry(DeviceClient* device, MainWindowChrome* chrome, QObject* parent)
    : QObject(parent)
    , device_(device)
    , chrome_(chrome)
{
    setupPanels();
}

void MainWindowPanelRegistry::setSpectrumAnalysisCoordinator(SpectrumAnalysisCoordinator* coordinator)
{
    spectrumAnalysisCoordinator_ = coordinator;
    if (chrome_ && chrome_->panelTitle()) {
        chrome_->panelTitle()->installEventFilter(this);
    }
}

void MainWindowPanelRegistry::selectPanel(int index)
{
    if (index < Dashboard || index > Log || !chrome_) return;

    currentPanel_ = index;
    if (spectrumAnalysisCoordinator_) {
        spectrumAnalysisCoordinator_->setActive(index == SpectrumAnalysis);
    }

    if (index == Log) {
        chrome_->logPanel()->toggleExpanded();
        return;
    }

    chrome_->panelStack()->setCurrentIndex(index);
    chrome_->panelTitle()->setText(index == SpectrumAnalysis
                                       ? QString::fromUtf8("光谱分析")
                                       : QString::fromUtf8(kPanelNames[index]));
    chrome_->panelTitle()->setCursor(index == SpectrumAnalysis ? Qt::PointingHandCursor : Qt::ArrowCursor);

    selectAssociatedViewerChannel(index);

    if (index == SpectrumAnalysis && spectrumAnalysisCoordinator_) {
        spectrumAnalysisCoordinator_->openDialog();
    }
}

bool MainWindowPanelRegistry::eventFilter(QObject* obj, QEvent* event)
{
    if (chrome_ && obj == chrome_->panelTitle() && event->type() == QEvent::MouseButtonRelease) {
        if (currentPanel_ == SpectrumAnalysis && spectrumAnalysisCoordinator_) {
            spectrumAnalysisCoordinator_->openDialog();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}

void MainWindowPanelRegistry::setupPanels()
{
    auto* stack = chrome_->panelStack();
    dashPanel_ = new DashboardPanel(stack);
    connPanel_ = new ConnectionPanel(device_, stack);
    cameraPanel_ = new CameraPanel(device_, stack);
    mirrorPanel_ = new MirrorPanel(device_, stack);
    irPanel_ = new IrPanel(device_, stack);
    collectPanel_ = new CollectPanel(device_, stack);
    streamPanel_ = new StreamPanel(device_, stack);
    spectralPanel_ = new SpectralPanel(stack);
    recordPlaybackPanel_ = new RecordPlaybackPanel(device_, stack);
    spectrumAnalysisPanel_ = new SpectrumAnalysisPanel(stack);

    stack->addWidget(wrapInScroll(dashPanel_));
    stack->addWidget(wrapInScroll(connPanel_));
    stack->addWidget(wrapInScroll(cameraPanel_));
    stack->addWidget(wrapInScroll(mirrorPanel_));
    stack->addWidget(wrapInScroll(irPanel_));
    stack->addWidget(wrapInScroll(collectPanel_));
    stack->addWidget(wrapInScroll(streamPanel_));
    stack->addWidget(wrapInScroll(spectralPanel_));
    stack->addWidget(wrapInScroll(recordPlaybackPanel_));
    stack->addWidget(wrapInScroll(spectrumAnalysisPanel_));
}

int MainWindowPanelRegistry::preferredStreamViewerChannel() const
{
    auto* viewer = chrome_->viewerArea();
    if (viewer->hasChannelImage(ViewerAreaWidget::SliceStitch16View)) {
        return ViewerAreaWidget::SliceStitch16View;
    }
    if (viewer->hasChannelImage(ViewerAreaWidget::SpectralPreviewView)) {
        return ViewerAreaWidget::SpectralPreviewView;
    }
    return ViewerAreaWidget::Raw16View;
}

void MainWindowPanelRegistry::selectAssociatedViewerChannel(int panelIndex)
{
    auto* viewer = chrome_->viewerArea();
    switch (panelIndex) {
    case Stream:
        viewer->setCurrentChannel(preferredStreamViewerChannel());
        break;
    case Spectral:
        viewer->setCurrentChannel(ViewerAreaWidget::SpectralView);
        break;
    case RecordPlayback:
        viewer->setCurrentChannel(ViewerAreaWidget::PlaybackView);
        break;
    case SpectrumAnalysis:
        viewer->setCurrentChannel(ViewerAreaWidget::SliceStitch16View);
        break;
    default:
        break;
    }
}

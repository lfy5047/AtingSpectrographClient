#include "MainWindowPanelRegistry.h"

#include "DeviceClient.h"
#include "MainWindowChrome.h"
#include "SpectrumAnalysisCoordinator.h"
#include "panels/AdvancedSettingsPanel.h"
#include "panels/BinningTestPanel.h"
#include "panels/CalibrationPanel.h"
#include "panels/DashboardPanel.h"
#include "panels/DataAcquisitionPanel.h"
#include "panels/IrPanel.h"
#include "panels/LogPanel.h"
#include "panels/RecordPlaybackPanel.h"
#include "panels/RoiTestPanel.h"
#include "panels/SpectrumAnalysisPanel.h"
#include "panels/SpectralSegmentTestPanel.h"
#include "panels/TempControlPanel.h"
#include "widgets/ViewerAreaWidget.h"

#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>

namespace {
const char* kPanelNames[] = {
    "仪表盘",
    "数据采集",
    "探测器设置",
    "校正",
    "温控控制",
    "录制回放",
    "光谱分析",
    "高级设置",
    "光谱段测试",
    "Binning 测试",
    "ROI 测试",
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
    chrome_->viewerArea()->setSpectralSegmentTestEnabled(index == SpectralSegmentTest);

    if (index == Log) {
        chrome_->logPanel()->toggleExpanded();
        return;
    }

    chrome_->panelStack()->setCurrentIndex(index);
    chrome_->panelTitle()->setText(QString::fromUtf8(kPanelNames[index]));
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
    dataAcquisitionPanel_ = new DataAcquisitionPanel(device_, stack);
    detectorPanel_ = new IrPanel(device_, stack);
    calibrationPanel_ = new CalibrationPanel(device_, stack);
    tempControlPanel_ = new TempControlPanel(device_, stack);
    recordPlaybackPanel_ = new RecordPlaybackPanel(device_, stack);
    spectrumAnalysisPanel_ = new SpectrumAnalysisPanel(stack);
    advancedSettingsPanel_ = new AdvancedSettingsPanel(device_, stack);
    spectralSegmentTestPanel_ = new SpectralSegmentTestPanel(stack);
    binningTestPanel_ = new BinningTestPanel(stack);
    roiTestPanel_ = new RoiTestPanel(stack);

    stack->addWidget(wrapInScroll(dashPanel_));
    stack->addWidget(wrapInScroll(dataAcquisitionPanel_));
    stack->addWidget(wrapInScroll(detectorPanel_));
    stack->addWidget(wrapInScroll(calibrationPanel_));
    stack->addWidget(wrapInScroll(tempControlPanel_));
    stack->addWidget(wrapInScroll(recordPlaybackPanel_));
    stack->addWidget(wrapInScroll(spectrumAnalysisPanel_));
    stack->addWidget(wrapInScroll(advancedSettingsPanel_));
    stack->addWidget(wrapInScroll(spectralSegmentTestPanel_));
    stack->addWidget(wrapInScroll(binningTestPanel_));
    stack->addWidget(wrapInScroll(roiTestPanel_));

    connect(chrome_->viewerArea(), &ViewerAreaWidget::spectralSegmentPositionsChanged,
            spectralSegmentTestPanel_, &SpectralSegmentTestPanel::setLinePositions);
}

ConnectionPanel* MainWindowPanelRegistry::connection() const
{
    return advancedSettingsPanel_ ? advancedSettingsPanel_->connection() : nullptr;
}

CameraPanel* MainWindowPanelRegistry::camera() const
{
    return advancedSettingsPanel_ ? advancedSettingsPanel_->camera() : nullptr;
}

CollectPanel* MainWindowPanelRegistry::collect() const
{
    return dataAcquisitionPanel_ ? dataAcquisitionPanel_->collect() : nullptr;
}

MirrorPanel* MainWindowPanelRegistry::mirror() const
{
    return dataAcquisitionPanel_ ? dataAcquisitionPanel_->mirror() : nullptr;
}

StreamPanel* MainWindowPanelRegistry::stream() const
{
    return dataAcquisitionPanel_ ? dataAcquisitionPanel_->stream() : nullptr;
}

SpectralPanel* MainWindowPanelRegistry::spectral() const
{
    return dataAcquisitionPanel_ ? dataAcquisitionPanel_->spectral() : nullptr;
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
    case DataAcquisition:
        viewer->setCurrentChannel(preferredStreamViewerChannel());
        break;
    case RecordPlayback:
        viewer->setCurrentChannel(ViewerAreaWidget::PlaybackView);
        break;
    case SpectrumAnalysis:
        viewer->setCurrentChannel(ViewerAreaWidget::SliceStitch16View);
        break;
    case SpectralSegmentTest:
        viewer->setCurrentChannel(ViewerAreaWidget::Raw16View);
        break;
    case BinningTest:
        viewer->setCurrentChannel(ViewerAreaWidget::BinningCompareView);
        break;
    case RoiTest:
        viewer->setCurrentChannel(ViewerAreaWidget::RoiCompareView);
        break;
    default:
        break;
    }
}

#include "SpectrumAnalysisCoordinator.h"

#include "SpectrumCurveDialog.h"
#include "panels/SpectrumAnalysisPanel.h"
#include "widgets/ViewerAreaWidget.h"

#include <QColor>
#include <QVector>
#include <cmath>

namespace {
QVector<int> spectrumSampledXs(int startX, int endX, int maxPlotPoints)
{
    QVector<int> xs;
    const int total = endX - startX + 1;
    if (total <= 0) return xs;

    maxPlotPoints = qBound(2, maxPlotPoints, total);
    xs.reserve(maxPlotPoints);
    if (total <= maxPlotPoints) {
        for (int x = startX; x <= endX; ++x) {
            xs.append(x);
        }
        return xs;
    }

    const double step = static_cast<double>(endX - startX) / static_cast<double>(maxPlotPoints - 1);
    for (int i = 0; i < maxPlotPoints; ++i) {
        int x = static_cast<int>(std::round(startX + i * step));
        if (i == 0) x = startX;
        if (i == maxPlotPoints - 1) x = endX;
        xs.append(qBound(startX, x, endX));
    }
    return xs;
}

double spectrumMovingAverage(const quint16* pixels, int rowOffset, int width, int x, int windowPixels)
{
    windowPixels = qMax(1, windowPixels);
    const int halfWindow = windowPixels / 2;
    const int left = qMax(0, x - halfWindow);
    const int right = qMin(width - 1, x + halfWindow);

    quint64 sum = 0;
    for (int sx = left; sx <= right; ++sx) {
        sum += pixels[rowOffset + sx];
    }
    return static_cast<double>(sum) / static_cast<double>(right - left + 1);
}
} // namespace

SpectrumAnalysisCoordinator::SpectrumAnalysisCoordinator(SpectrumAnalysisPanel* panel,
                                                         ViewerAreaWidget* viewerArea,
                                                         QWidget* dialogParent,
                                                         QObject* parent)
    : QObject(parent)
    , panel_(panel)
    , viewerArea_(viewerArea)
    , dialogParent_(dialogParent)
{
    if (viewerArea_) {
        currentChannel_ = viewerArea_->currentChannel();
    }
    setupConnections();
}

void SpectrumAnalysisCoordinator::setActive(bool active)
{
    if (active_ == active) {
        refreshOverlay();
        return;
    }
    active_ = active;
    refreshOverlay();
}

void SpectrumAnalysisCoordinator::setCurrentChannel(int channel)
{
    if (currentChannel_ == channel) {
        refreshOverlay();
        return;
    }
    currentChannel_ = channel;
    refreshOverlay();
}

void SpectrumAnalysisCoordinator::setLatestSliceFrame(int width, int height, quint64 frameId, const QByteArray& data)
{
    latestSliceData_ = data;
    latestSliceWidth_ = width;
    latestSliceHeight_ = height;
    latestSliceFrameId_ = frameId;
    if (panel_) {
        panel_->setSliceGeometry(width, height, true);
    }
    refreshOverlay();
}

void SpectrumAnalysisCoordinator::openDialog()
{
    if (!panel_) return;

    if (!dialog_) {
        dialog_ = new SpectrumCurveDialog(dialogParent_);
        dialog_->setRefreshRateHz(panel_->refreshRateHz());
        connect(dialog_, &SpectrumCurveDialog::sampleRefreshRequested,
                this, &SpectrumAnalysisCoordinator::updateCurveData);
    }

    dialog_->show();
    dialog_->raise();
    dialog_->activateWindow();
    refreshOverlay();
    forceRefreshCurves();
}

void SpectrumAnalysisCoordinator::refreshOverlay()
{
    if (!viewerArea_) return;
    const bool enabled = active_ && currentChannel_ == ViewerAreaWidget::SliceStitch16View;
    viewerArea_->setSliceAnalysisOverlay(enabled, panel_ ? panel_->lines() : QVector<SpectrumSampleLine>());
}

void SpectrumAnalysisCoordinator::forceRefreshCurves()
{
    updateCurveData(true);
}

void SpectrumAnalysisCoordinator::setupConnections()
{
    if (!panel_ || !viewerArea_) return;

    connect(viewerArea_, &ViewerAreaWidget::channelChanged,
            this, &SpectrumAnalysisCoordinator::setCurrentChannel);
    connect(viewerArea_, &ViewerAreaWidget::sliceAnalysisLineAddRequested,
            panel_, &SpectrumAnalysisPanel::addLineAt);
    connect(viewerArea_, &ViewerAreaWidget::sliceAnalysisLineMoveRequested,
            panel_, &SpectrumAnalysisPanel::moveLine);
    connect(viewerArea_, &ViewerAreaWidget::sliceAnalysisLineDeleteRequested,
            panel_, &SpectrumAnalysisPanel::deleteLine);
    connect(panel_, &SpectrumAnalysisPanel::linesChanged, this, [this]() {
        refreshOverlay();
        forceRefreshCurves();
    });
    connect(panel_, &SpectrumAnalysisPanel::settingsChanged, this, [this]() {
        if (dialog_) {
            dialog_->setRefreshRateHz(panel_->refreshRateHz());
        }
        forceRefreshCurves();
    });
    connect(panel_, &SpectrumAnalysisPanel::showDialogRequested,
            this, &SpectrumAnalysisCoordinator::openDialog);
}

void SpectrumAnalysisCoordinator::updateCurveData(bool force)
{
    if (!panel_ || !dialog_) return;
    const double yRangeMultiplier = panel_->yRangeMultiplier();
    const double yMinPositionPercent = panel_->yMinPositionPercent();
    const double yMinDataSpan = panel_->yMinDataSpan();

    if (latestSliceData_.isEmpty() || latestSliceWidth_ <= 0 || latestSliceHeight_ <= 0) {
        panel_->setStatusText(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"));
        dialog_->setStatusText(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"));
        dialog_->setCurveData(QVector<QVector<double>>(),
                              QVector<QVector<double>>(),
                              QVector<QString>(),
                              QVector<QColor>(),
                              QString::fromUtf8("波长"),
                              yRangeMultiplier,
                              yMinPositionPercent,
                              yMinDataSpan);
        return;
    }
    if (!force && lastCurveFrameId_ == latestSliceFrameId_) {
        return;
    }

    const QVector<SpectrumSampleLine> lines = panel_->lines();
    if (lines.isEmpty()) {
        dialog_->setCurveData(QVector<QVector<double>>(),
                              QVector<QVector<double>>(),
                              QVector<QString>(),
                              QVector<QColor>(),
                              QString::fromUtf8("波长"),
                              yRangeMultiplier,
                              yMinPositionPercent,
                              yMinDataSpan);
        panel_->setStatusText(QString::fromUtf8("点击 SliceStitch16 图像添加采样线"));
        lastCurveFrameId_ = latestSliceFrameId_;
        return;
    }

    const int x0 = qBound(0, panel_->xStart(), latestSliceWidth_ - 1);
    const int x1 = qBound(0, panel_->xEnd(), latestSliceWidth_ - 1);
    const int startX = qMin(x0, x1);
    const int endX = qMax(x0, x1);
    if (startX == endX || latestSliceData_.size() < latestSliceWidth_ * latestSliceHeight_ * 2) {
        dialog_->setStatusText(QString::fromUtf8("映射范围或图像数据无效"));
        return;
    }

    const double waveStart = panel_->wavelengthStart();
    const double waveEnd = panel_->wavelengthEnd();
    const double denom = static_cast<double>(endX - startX);
    const int filterWindow = panel_->filterWindowPixels();
    const int maxPlotPoints = panel_->maxPlotPoints();
    const QVector<int> sampledXs = spectrumSampledXs(startX, endX, maxPlotPoints);
    const auto* pixels = reinterpret_cast<const quint16*>(latestSliceData_.constData());

    QVector<QVector<double>> xList;
    QVector<QVector<double>> yList;
    QVector<QString> names;
    QVector<QColor> colors;
    for (const SpectrumSampleLine& line : lines) {
        const int y = qBound(0, line.y, latestSliceHeight_ - 1);
        QVector<double> xs;
        QVector<double> ys;
        xs.reserve(sampledXs.size());
        ys.reserve(sampledXs.size());
        const int rowOffset = y * latestSliceWidth_;
        for (int x : sampledXs) {
            const double t = static_cast<double>(x - startX) / denom;
            xs.append(waveStart + t * (waveEnd - waveStart));
            ys.append(spectrumMovingAverage(pixels, rowOffset, latestSliceWidth_, x, filterWindow));
        }
        xList.append(xs);
        yList.append(ys);
        names.append(line.name());
        colors.append(line.color);
    }

    dialog_->setCurveData(xList, yList, names, colors, QString::fromUtf8("波长"),
                          yRangeMultiplier,
                          yMinPositionPercent,
                          yMinDataSpan);
    panel_->setStatusText(QString::fromUtf8("已更新 %1 条曲线，帧 %2")
                              .arg(lines.size())
                              .arg(latestSliceFrameId_));
    lastCurveFrameId_ = latestSliceFrameId_;
}

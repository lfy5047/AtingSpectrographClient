#include "SpectrumCurveDialog.h"

#include "ThemeManager.h"
#include "widgets/qcustomplot.h"

#include <QCloseEvent>
#include <QEvent>
#include <QLabel>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include <limits>

namespace {
const char* kGeometryKey = "spectrumAnalysis/dialogGeometry";
}

SpectrumCurveDialog::SpectrumCurveDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("光谱强度曲线"));
    setObjectName("spectrumCurveDialog");
    resize(980, 620);
    setMinimumSize(720, 420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    statusLabel_ = new QLabel(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"), this);
    statusLabel_->setObjectName("spectrumCurveStatus");
    root->addWidget(statusLabel_);

    plot_ = new QCustomPlot(this);
    plot_->setObjectName("spectrumCurvePlot");
    root->addWidget(plot_, 1);
    applyPlotTheme();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        emit sampleRefreshRequested(false);
    });
    setRefreshRateHz(10);
}

void SpectrumCurveDialog::setRefreshRateHz(int hz)
{
    hz = qBound(1, hz, 30);
    refreshTimer_->setInterval(qMax(1, 1000 / hz));
    if (isVisible() && !refreshTimer_->isActive()) {
        refreshTimer_->start();
    }
}

void SpectrumCurveDialog::setStatusText(const QString& text)
{
    if (statusLabel_) statusLabel_->setText(text);
}

void SpectrumCurveDialog::setCurveData(const QVector<QVector<double>>& xList,
                                       const QVector<QVector<double>>& yList,
                                       const QVector<QString>& names,
                                       const QVector<QColor>& colors,
                                       const QString& xAxisLabel,
                                       double yRangeMultiplier,
                                       double yMinPositionPercent,
                                       double yMinDataSpan)
{
    if (!plot_) return;
    const int count = qMin(qMin(xList.size(), yList.size()), qMin(names.size(), colors.size()));
    while (plot_->graphCount() < count) {
        plot_->addGraph();
    }
    while (plot_->graphCount() > count) {
        plot_->removeGraph(plot_->graphCount() - 1);
    }

    for (int i = 0; i < count; ++i) {
        QCPGraph* graph = plot_->graph(i);
        graph->setData(xList[i], yList[i], true);
        graph->setName(names[i]);
        graph->setPen(QPen(colors[i], 2));
        graph->setLineStyle(QCPGraph::lsLine);
        graph->setSmooth(true);
    }

    plot_->xAxis->setLabel(xAxisLabel);
    plot_->yAxis->setLabel("DN");
    plot_->legend->setVisible(count > 0);
    if (count > 0) {
        plot_->xAxis->rescale();

        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();
        for (int i = 0; i < count; ++i) {
            for (double value : yList[i]) {
                minY = qMin(minY, value);
                maxY = qMax(maxY, value);
            }
        }

        if (minY <= maxY) {
            const double dataDiff = qMax(0.0, maxY - minY);
            const double minDataSpan = qMax(1.0, yMinDataSpan);
            const double effectiveDataSpan = dataDiff < minDataSpan
                ? minDataSpan
                : dataDiff * qMax(1.05, yRangeMultiplier);
            const double minFraction = qBound(0.0, yMinPositionPercent / 100.0, 0.30);
            const double axisSpan = effectiveDataSpan / (1.0 - 2.0 * minFraction);
            const double axisLower = minY - axisSpan * minFraction;
            plot_->yAxis->setRange(axisLower, axisLower + axisSpan);
        } else {
            plot_->yAxis->setRange(0, 65535);
        }
        setStatusText(QString::fromUtf8("已更新 %1 条曲线").arg(count));
    } else {
        plot_->xAxis->setRange(0, 1);
        plot_->yAxis->setRange(0, 65535);
    }
    plot_->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumCurveDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event && event->type() == QEvent::StyleChange) {
        applyPlotTheme();
    }
}

void SpectrumCurveDialog::closeEvent(QCloseEvent* event)
{
    saveGeometrySetting();
    if (refreshTimer_) refreshTimer_->stop();
    QDialog::closeEvent(event);
}

void SpectrumCurveDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    loadGeometry();
    if (refreshTimer_ && !refreshTimer_->isActive()) refreshTimer_->start();
    emit sampleRefreshRequested(true);
}

void SpectrumCurveDialog::loadGeometry()
{
    if (geometryLoaded_) return;
    geometryLoaded_ = true;
    QSettings s;
    const QByteArray geometry = s.value(kGeometryKey).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
}

void SpectrumCurveDialog::saveGeometrySetting() const
{
    QSettings s;
    s.setValue(kGeometryKey, saveGeometry());
}

void SpectrumCurveDialog::applyPlotTheme()
{
    const bool outdoor = ThemeManager::currentTheme() == ThemeManager::Theme::OutdoorLight;
    const QColor plotBg = outdoor ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x12, 0x17, 0x1D);
    const QColor legendBg = outdoor ? QColor(0xFF, 0xFF, 0xFF, 235) : QColor(0x16, 0x1B, 0x23, 230);
    const QColor borderColor = outdoor ? QColor(0xC9, 0xD4, 0xE2) : QColor(0x21, 0x26, 0x2E);
    const QColor axisColor = outdoor ? QColor(0x4B, 0x55, 0x63) : QColor(0x7D, 0x85, 0x90);
    const QColor labelColor = outdoor ? QColor(0x11, 0x18, 0x27) : QColor(0xE6, 0xED, 0xF3);
    const QColor gridColor = outdoor ? QColor(0xD8, 0xE0, 0xEA) : QColor(0x21, 0x26, 0x2E);

    plot_->setBackground(QBrush(plotBg));
    plot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    plot_->legend->setBrush(QBrush(legendBg));
    plot_->legend->setBorderPen(QPen(borderColor));
    plot_->legend->setTextColor(labelColor);

    for (QCPAxis* axis : {plot_->xAxis, plot_->yAxis}) {
        axis->setBasePen(QPen(axisColor));
        axis->setTickPen(QPen(axisColor));
        axis->setSubTickPen(QPen(axisColor));
        axis->setTickLabelColor(labelColor);
        axis->setLabelColor(labelColor);
        axis->grid()->setPen(QPen(gridColor));
        axis->grid()->setSubGridVisible(false);
    }
    plot_->xAxis->setLabel(QString::fromUtf8("波长"));
    plot_->yAxis->setLabel("DN");
    plot_->replot(QCustomPlot::rpQueuedReplot);
}

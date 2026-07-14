#include "RoiTestPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QString configText(const RoiConfig& config)
{
    return QStringLiteral("[%1, %2) x [%3, %4)")
        .arg(config.sliceBegin).arg(config.sliceEnd)
        .arg(config.sliceHBegin).arg(config.sliceHEnd);
}

QString sizeText(const QSize& size)
{
    return size.isValid() ? QStringLiteral("%1x%2").arg(size.width()).arg(size.height())
                          : QStringLiteral("--");
}

} // namespace

RoiTestPanel::RoiTestPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("roiTestPanel"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    auto* instruction = new QLabel(
        QString::fromUtf8("一键测试会临时启用静态采集，先采集全幅基准，再应用 ROI 并采集对比图，完成后恢复测试前配置。"),
        this);
    instruction->setWordWrap(true);
    root->addWidget(instruction);

    auto* configGroup = new QGroupBox(QString::fromUtf8("ROI 配置"), this);
    auto* configForm = new QFormLayout(configGroup);
    currentConfigLabel_ = new QLabel(QStringLiteral("--"), configGroup);
    currentConfigLabel_->setObjectName(QStringLiteral("roiCurrentConfigLabel"));
    currentConfigLabel_->setWordWrap(true);
    currentConfigLabel_->setProperty("readout", true);
    resolutionLabel_ = new QLabel(QStringLiteral("--"), configGroup);
    resolutionLabel_->setObjectName(QStringLiteral("roiResolutionLabel"));
    resolutionLabel_->setProperty("readout", true);
    configForm->addRow(QString::fromUtf8("当前配置"), currentConfigLabel_);
    configForm->addRow(QString::fromUtf8("全幅尺寸"), resolutionLabel_);

    auto createSpin = [configGroup](const QString& name, int value) {
        auto* spin = new QSpinBox(configGroup);
        spin->setObjectName(name);
        spin->setRange(0, 65535);
        spin->setValue(value);
        return spin;
    };
    sliceBeginSpin_ = createSpin(QStringLiteral("roiSliceBeginSpin"), 240);
    sliceEndSpin_ = createSpin(QStringLiteral("roiSliceEndSpin"), 435);
    sliceHBeginSpin_ = createSpin(QStringLiteral("roiSliceHBeginSpin"), 0);
    sliceHEndSpin_ = createSpin(QStringLiteral("roiSliceHEndSpin"), 512);
    configForm->addRow(QString::fromUtf8("光谱列起点"), sliceBeginSpin_);
    configForm->addRow(QString::fromUtf8("光谱列终点（不含）"), sliceEndSpin_);
    configForm->addRow(QString::fromUtf8("空间行起点"), sliceHBeginSpin_);
    configForm->addRow(QString::fromUtf8("空间行终点（不含）"), sliceHEndSpin_);

    refreshBtn_ = new QPushButton(QString::fromUtf8("读取"), configGroup);
    refreshBtn_->setObjectName(QStringLiteral("roiRefreshButton"));
    applyBtn_ = new QPushButton(QString::fromUtf8("应用"), configGroup);
    applyBtn_->setObjectName(QStringLiteral("roiApplyButton"));
    auto* configActions = new QHBoxLayout();
    configActions->addWidget(refreshBtn_);
    configActions->addWidget(applyBtn_);
    configForm->addRow(QString(), configActions);
    root->addWidget(configGroup);

    auto* actionGroup = new QGroupBox(QString::fromUtf8("自动测试"), this);
    auto* actionLayout = new QVBoxLayout(actionGroup);
    auto* actionRow = new QHBoxLayout();
    startBtn_ = new QPushButton(QString::fromUtf8("一键静态采集测试"), actionGroup);
    startBtn_->setObjectName(QStringLiteral("roiStartTestButton"));
    startBtn_->setProperty("primary", true);
    cancelBtn_ = new QPushButton(QString::fromUtf8("取消"), actionGroup);
    cancelBtn_->setObjectName(QStringLiteral("roiCancelTestButton"));
    cancelBtn_->setEnabled(false);
    actionRow->addWidget(startBtn_, 1);
    actionRow->addWidget(cancelBtn_);
    actionLayout->addLayout(actionRow);

    progressBar_ = new QProgressBar(actionGroup);
    progressBar_->setObjectName(QStringLiteral("roiTestProgressBar"));
    progressBar_->setRange(0, 2);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    actionLayout->addWidget(progressBar_);
    statusLabel_ = new QLabel(QString::fromUtf8("连接设备后可开始测试"), actionGroup);
    statusLabel_->setObjectName(QStringLiteral("roiStatusLabel"));
    statusLabel_->setWordWrap(true);
    actionLayout->addWidget(statusLabel_);
    root->addWidget(actionGroup);

    auto* resultGroup = new QGroupBox(QString::fromUtf8("采集结果"), this);
    auto* resultLayout = new QVBoxLayout(resultGroup);
    resultTable_ = new QTableWidget(2, 4, resultGroup);
    resultTable_->setObjectName(QStringLiteral("roiResultTable"));
    resultTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("阶段"), QString::fromUtf8("配置"),
        QString::fromUtf8("理论尺寸"), QString::fromUtf8("实际尺寸"),
    });
    resultTable_->verticalHeader()->hide();
    resultTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    resultTable_->horizontalHeader()->setStretchLastSection(true);
    resultTable_->setSelectionMode(QAbstractItemView::NoSelection);
    resultTable_->setFocusPolicy(Qt::NoFocus);
    resultLayout->addWidget(resultTable_);
    root->addWidget(resultGroup);
    root->addStretch();

    resetResults();
    connect(refreshBtn_, &QPushButton::clicked, this, &RoiTestPanel::refreshRequested);
    connect(applyBtn_, &QPushButton::clicked, this, [this]() {
        emit applyRequested(testConfig());
    });
    connect(startBtn_, &QPushButton::clicked, this, &RoiTestPanel::startRequested);
    connect(cancelBtn_, &QPushButton::clicked, this, &RoiTestPanel::cancelRequested);
}

RoiConfig RoiTestPanel::testConfig() const
{
    RoiConfig config;
    config.sliceBegin = sliceBeginSpin_->value();
    config.sliceEnd = sliceEndSpin_->value();
    config.sliceHBegin = sliceHBeginSpin_->value();
    config.sliceHEnd = sliceHEndSpin_->value();
    return config;
}

void RoiTestPanel::setCurrentConfig(const RoiConfig& config)
{
    QString suffix;
    if (config.pendingApply) suffix = QString::fromUtf8("（待下一采集段生效）");
    currentConfigLabel_->setText(configText(config) + suffix);
}

void RoiTestPanel::setResolution(const QSize& resolution)
{
    resolutionLabel_->setText(sizeText(resolution));
    if (!resolution.isValid()) return;
    sliceBeginSpin_->setMaximum(qMax(0, resolution.width() - 1));
    sliceEndSpin_->setMaximum(resolution.width());
    sliceHBeginSpin_->setMaximum(qMax(0, resolution.height() - 1));
    sliceHEndSpin_->setMaximum(resolution.height());
}

void RoiTestPanel::setBusy(bool busy, bool cancellable)
{
    sliceBeginSpin_->setEnabled(!busy);
    sliceEndSpin_->setEnabled(!busy);
    sliceHBeginSpin_->setEnabled(!busy);
    sliceHEndSpin_->setEnabled(!busy);
    refreshBtn_->setEnabled(!busy);
    applyBtn_->setEnabled(!busy);
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy && cancellable);
}

void RoiTestPanel::setStatusText(const QString& text, bool error)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("error", error);
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

void RoiTestPanel::setProgress(int completedSteps, const QString& text)
{
    progressBar_->setValue(qBound(0, completedSteps, 2));
    progressBar_->setFormat(text);
}

void RoiTestPanel::resetResults()
{
    resultTable_->setItem(0, 0, readOnlyItem(QString::fromUtf8("全幅基准")));
    resultTable_->setItem(1, 0, readOnlyItem(QStringLiteral("ROI")));
    for (int row = 0; row < 2; ++row) {
        for (int column = 1; column < 4; ++column) {
            resultTable_->setItem(row, column, readOnlyItem(QStringLiteral("--")));
        }
    }
}

void RoiTestPanel::setCaptureResult(bool fullFrame, const RoiConfig& config,
                                    const QSize& expected, const QSize& actual)
{
    const int row = fullFrame ? 0 : 1;
    resultTable_->item(row, 1)->setText(configText(config));
    resultTable_->item(row, 2)->setText(sizeText(expected));
    resultTable_->item(row, 3)->setText(sizeText(actual));
}

#include "BinningTestPanel.h"

#include "Protocol.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QString sizeText(const QSize& size)
{
    return size.isValid() ? QStringLiteral("%1x%2").arg(size.width()).arg(size.height())
                          : QStringLiteral("--");
}

QTableWidgetItem* readOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

} // namespace

BinningTestPanel::BinningTestPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("binningTestPanel"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    auto* instruction = new QLabel(
        QString::fromUtf8("保持标准靶标、视场和转镜静止。自动测试将依次采集 1x1、2x2、4x4，并在完成后恢复原配置。"),
        this);
    instruction->setWordWrap(true);
    root->addWidget(instruction);

    auto* configGroup = new QGroupBox(QString::fromUtf8("Binning 配置"), this);
    auto* configForm = new QFormLayout(configGroup);
    currentConfigLabel_ = new QLabel(QStringLiteral("--"), configGroup);
    currentConfigLabel_->setObjectName(QStringLiteral("binningCurrentConfigLabel"));
    currentConfigLabel_->setProperty("readout", true);
    configForm->addRow(QString::fromUtf8("当前配置"), currentConfigLabel_);

    sourceCombo_ = new QComboBox(configGroup);
    sourceCombo_->setObjectName(QStringLiteral("binningSourceCombo"));
    sourceCombo_->addItem(QStringLiteral("Raw16"), static_cast<int>(cli::proto::Raw16));
    sourceCombo_->addItem(QStringLiteral("SliceStitch16"),
                          static_cast<int>(cli::proto::SliceStitch16));
    configForm->addRow(QString::fromUtf8("数据源"), sourceCombo_);

    factorCombo_ = new QComboBox(configGroup);
    factorCombo_->setObjectName(QStringLiteral("binningFactorCombo"));
    factorCombo_->addItem(QStringLiteral("1x1"), 1);
    factorCombo_->addItem(QStringLiteral("2x2"), 2);
    factorCombo_->addItem(QStringLiteral("4x4"), 4);
    applyBtn_ = new QPushButton(QString::fromUtf8("应用"), configGroup);
    applyBtn_->setObjectName(QStringLiteral("binningApplyFactorButton"));
    refreshBtn_ = new QPushButton(QString::fromUtf8("读取"), configGroup);
    refreshBtn_->setObjectName(QStringLiteral("binningRefreshButton"));
    auto* factorRow = new QHBoxLayout();
    factorRow->addWidget(factorCombo_, 1);
    factorRow->addWidget(refreshBtn_);
    factorRow->addWidget(applyBtn_);
    configForm->addRow(QString::fromUtf8("测试倍率"), factorRow);

    orientationCombo_ = new QComboBox(configGroup);
    orientationCombo_->setObjectName(QStringLiteral("binningMeasurementOrientationCombo"));
    orientationCombo_->addItem(QString::fromUtf8("光谱纬度（竖线）"), static_cast<int>(Qt::Horizontal));
    orientationCombo_->addItem(QString::fromUtf8("空间纬度（横线）"), static_cast<int>(Qt::Vertical));
    configForm->addRow(QString::fromUtf8("特征测量"), orientationCombo_);
    root->addWidget(configGroup);

    auto* actionGroup = new QGroupBox(QString::fromUtf8("自动测试"), this);
    auto* actionLayout = new QVBoxLayout(actionGroup);
    auto* actionRow = new QHBoxLayout();
    startBtn_ = new QPushButton(QString::fromUtf8("采集三组对比"), actionGroup);
    startBtn_->setObjectName(QStringLiteral("binningStartTestButton"));
    startBtn_->setProperty("primary", true);
    cancelBtn_ = new QPushButton(QString::fromUtf8("取消"), actionGroup);
    cancelBtn_->setObjectName(QStringLiteral("binningCancelTestButton"));
    cancelBtn_->setEnabled(false);
    actionRow->addWidget(startBtn_, 1);
    actionRow->addWidget(cancelBtn_);
    actionLayout->addLayout(actionRow);

    progressBar_ = new QProgressBar(actionGroup);
    progressBar_->setObjectName(QStringLiteral("binningTestProgressBar"));
    progressBar_->setRange(0, 3);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    actionLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel(QString::fromUtf8("连接设备并订阅所选数据源后开始测试"), actionGroup);
    statusLabel_->setObjectName(QStringLiteral("binningStatusLabel"));
    statusLabel_->setWordWrap(true);
    actionLayout->addWidget(statusLabel_);
    root->addWidget(actionGroup);

    auto* resultGroup = new QGroupBox(QString::fromUtf8("测试结果"), this);
    auto* resultLayout = new QVBoxLayout(resultGroup);
    resultTable_ = new QTableWidget(3, 5, resultGroup);
    resultTable_->setObjectName(QStringLiteral("binningResultTable"));
    resultTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("模式"), QString::fromUtf8("配置"),
        QString::fromUtf8("理论尺寸"), QString::fromUtf8("实际尺寸"),
        QString::fromUtf8("特征宽度"),
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

    connect(refreshBtn_, &QPushButton::clicked, this, &BinningTestPanel::refreshRequested);
    connect(applyBtn_, &QPushButton::clicked, this, [this]() {
        emit applyFactorRequested(factorCombo_->currentData().toInt());
    });
    connect(startBtn_, &QPushButton::clicked, this, &BinningTestPanel::startRequested);
    connect(cancelBtn_, &QPushButton::clicked, this, &BinningTestPanel::cancelRequested);
    connect(sourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit sourceChannelChanged(sourceChannel());
    });
    connect(orientationCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit measurementOrientationChanged(
            static_cast<Qt::Orientation>(orientationCombo_->currentData().toInt()));
    });
}

int BinningTestPanel::sourceChannel() const
{
    return sourceCombo_ ? sourceCombo_->currentData().toInt()
                        : static_cast<int>(cli::proto::Raw16);
}

void BinningTestPanel::setCurrentConfig(const BinningConfig& config)
{
    currentConfigLabel_->setText(QStringLiteral("%1x%2 (%3)")
        .arg(config.spectralFactor)
        .arg(config.spatialFactor)
        .arg(config.enabled ? QString::fromUtf8("启用") : QString::fromUtf8("透传")));

    if (config.spectralFactor == config.spatialFactor) {
        const int index = factorCombo_->findData(config.spectralFactor);
        if (index >= 0) {
            const QSignalBlocker blocker(factorCombo_);
            factorCombo_->setCurrentIndex(index);
        }
    }
}

void BinningTestPanel::setBusy(bool busy, bool cancellable)
{
    sourceCombo_->setEnabled(!busy);
    factorCombo_->setEnabled(!busy);
    orientationCombo_->setEnabled(!busy);
    refreshBtn_->setEnabled(!busy);
    applyBtn_->setEnabled(!busy);
    startBtn_->setEnabled(!busy);
    cancelBtn_->setEnabled(busy && cancellable);
}

void BinningTestPanel::setStatusText(const QString& text, bool error)
{
    statusLabel_->setText(text);
    statusLabel_->setProperty("error", error);
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

void BinningTestPanel::setProgress(int completedSteps, const QString& text)
{
    progressBar_->setValue(qBound(0, completedSteps, 3));
    progressBar_->setFormat(text);
}

void BinningTestPanel::resetResults()
{
    const int factors[] = {1, 2, 4};
    for (int row = 0; row < 3; ++row) {
        resultTable_->setItem(row, 0, readOnlyItem(QStringLiteral("%1x%1").arg(factors[row])));
        for (int column = 1; column < 5; ++column) {
            resultTable_->setItem(row, column, readOnlyItem(QStringLiteral("--")));
        }
    }
}

void BinningTestPanel::setCaptureResult(int factor, bool configVerified,
                                        const QSize& expected, const QSize& actual)
{
    const int row = rowForFactor(factor);
    if (row < 0) return;
    resultTable_->item(row, 1)->setText(configVerified ? QString::fromUtf8("通过")
                                                       : QString::fromUtf8("失败"));
    resultTable_->item(row, 2)->setText(sizeText(expected));
    resultTable_->item(row, 3)->setText(sizeText(actual));
    resultTable_->item(row, 4)->setText(QString::fromUtf8("未测量"));
}

void BinningTestPanel::setMeasurementResult(int factor, int pixels)
{
    const int row = rowForFactor(factor);
    if (row < 0) return;
    resultTable_->item(row, 4)->setText(pixels >= 0
        ? QStringLiteral("%1 px").arg(pixels)
        : QStringLiteral("--"));
}

int BinningTestPanel::rowForFactor(int factor)
{
    if (factor == 1) return 0;
    if (factor == 2) return 1;
    if (factor == 4) return 2;
    return -1;
}

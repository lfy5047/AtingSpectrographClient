#include "RoiTestPanel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
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
        QString::fromUtf8("一键测试会临时启用静态采集，先采集全幅基准，然后采集窗口图；"),
        this);
    instruction->setWordWrap(true);
    root->addWidget(instruction);

    auto* optionGroup = new QGroupBox(QString::fromUtf8("测试选项"), this);
    auto* optionLayout = new QVBoxLayout(optionGroup);
    applyWindowingCheck_ = new QCheckBox(QString::fromUtf8("应用开窗功能"), optionGroup);
    applyWindowingCheck_->setObjectName(QStringLiteral("roiApplyWindowingCheck"));
    applyWindowingCheck_->setChecked(true);
    optionLayout->addWidget(applyWindowingCheck_);
    root->addWidget(optionGroup);

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
    resultTable_ = new QTableWidget(2, 3, resultGroup);
    resultTable_->setObjectName(QStringLiteral("roiResultTable"));
    resultTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("阶段"), QString::fromUtf8("理论尺寸"),
        QString::fromUtf8("实际尺寸"),
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
    connect(startBtn_, &QPushButton::clicked, this, &RoiTestPanel::startRequested);
    connect(cancelBtn_, &QPushButton::clicked, this, &RoiTestPanel::cancelRequested);
}

bool RoiTestPanel::applyWindowing() const
{
    return applyWindowingCheck_->isChecked();
}

void RoiTestPanel::setBusy(bool busy, bool cancellable)
{
    applyWindowingCheck_->setEnabled(!busy);
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
    resultTable_->setItem(1, 0, readOnlyItem(QString::fromUtf8("开窗图")));
    for (int row = 0; row < 2; ++row) {
        for (int column = 1; column < 3; ++column) {
            resultTable_->setItem(row, column, readOnlyItem(QStringLiteral("--")));
        }
    }
}

void RoiTestPanel::setCaptureResult(bool fullFrame, const QSize& expected, const QSize& actual)
{
    const int row = fullFrame ? 0 : 1;
    resultTable_->item(row, 1)->setText(sizeText(expected));
    resultTable_->item(row, 2)->setText(sizeText(actual));
}

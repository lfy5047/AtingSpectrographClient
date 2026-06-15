#include "CollectPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>

CollectPanel::CollectPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* grp = new QGroupBox(QString::fromUtf8("运动采集门控"), this);
    auto* fl = new QFormLayout(grp);

    statusLabel_ = new QLabel(QString::fromUtf8("未知"), this);
    statusLabel_->setProperty("readout", true);
    fl->addRow(QString::fromUtf8("状态"), statusLabel_);

    oversampleFactorSpin_ = new QSpinBox(this);
    oversampleFactorSpin_->setRange(1, 1024);
    oversampleFactorSpin_->setValue(1);
    applyOversamplingBtn_ = new QPushButton(QString::fromUtf8("应用"), this);
    auto* oversamplingRow = new QHBoxLayout();
    oversamplingRow->addWidget(oversampleFactorSpin_, 1);
    oversamplingRow->addWidget(applyOversamplingBtn_);
    fl->addRow(QString::fromUtf8("超采样倍率"), oversamplingRow);

    effectiveSSpeedLabel_ = new QLabel(QString::fromUtf8("-"), this);
    effectiveSSpeedLabel_->setProperty("readout", true);
    effectiveFSpeedLabel_ = new QLabel(QString::fromUtf8("-"), this);
    effectiveFSpeedLabel_->setProperty("readout", true);
    fl->addRow(QString::fromUtf8("有效 S 速度"), effectiveSSpeedLabel_);
    fl->addRow(QString::fromUtf8("有效 F 速度"), effectiveFSpeedLabel_);

    auto* row = new QHBoxLayout();
    startBtn_ = new QPushButton(QString::fromUtf8("开始"), this);
    startBtn_->setProperty("primary", true);
    stopBtn_ = new QPushButton(QString::fromUtf8("停止"), this);
    stopBtn_->setProperty("danger", true);
    refreshBtn_ = new QPushButton(QString::fromUtf8("刷新"), this);
    row->addWidget(startBtn_);
    row->addWidget(stopBtn_);
    row->addWidget(refreshBtn_);
    fl->addRow(row);

    root->addWidget(grp);
    root->addStretch();

    auto cb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "Collect", err);
        else refreshStatus();
    };

    connect(startBtn_, &QPushButton::clicked, this, [this, cb]() { dev_->collect()->start(this, cb); });
    connect(stopBtn_, &QPushButton::clicked, this, [this, cb]() { dev_->collect()->stop(this, cb); });
    connect(refreshBtn_, &QPushButton::clicked, this, &CollectPanel::refreshStatus);
    connect(applyOversamplingBtn_, &QPushButton::clicked, this, &CollectPanel::applyOversampling);

    connect(dev, &DeviceClient::connectionChanged, this, [this](bool c, const QString&) {
        if (c) refreshStatus();
    });
}

void CollectPanel::refreshStatus()
{
    dev_->collect()->getOversampling(this, [this](bool ok, const CollectOversamplingInfo& info, const QString&) {
        if (ok) {
            updateOversamplingUi(info);
        } else {
            statusLabel_->setText(QString::fromUtf8("未知"));
            effectiveSSpeedLabel_->setText(QString::fromUtf8("-"));
            effectiveFSpeedLabel_->setText(QString::fromUtf8("-"));
        }
    });
}

void CollectPanel::applyOversampling()
{
    const int factor = oversampleFactorSpin_->value();
    applyOversamplingBtn_->setEnabled(false);
    dev_->collect()->setOversampling(this, factor,
        [this](bool ok, const CollectOversamplingInfo& info, const QString& err) {
            if (ok) {
                updateOversamplingUi(info);
            } else {
                QMessageBox::warning(this, "Collect", err);
                applyOversamplingBtn_->setEnabled(true);
                refreshStatus();
            }
        });
}

void CollectPanel::updateOversamplingUi(const CollectOversamplingInfo& info)
{
    statusLabel_->setText(info.collecting
        ? QString::fromUtf8("采集中")
        : QString::fromUtf8("已停止"));
    {
        const QSignalBlocker blocker(oversampleFactorSpin_);
        oversampleFactorSpin_->setValue(info.oversampleFactor);
    }
    effectiveSSpeedLabel_->setText(QString::number(info.effectiveSSpeed));
    effectiveFSpeedLabel_->setText(QString::number(info.effectiveFSpeed));
    oversampleFactorSpin_->setEnabled(!info.collecting);
    applyOversamplingBtn_->setEnabled(!info.collecting);
}

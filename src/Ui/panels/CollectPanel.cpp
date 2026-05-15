#include "CollectPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>

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

    connect(startBtn_, &QPushButton::clicked, this, [this, cb]() { dev_->collectStart(cb); });
    connect(stopBtn_, &QPushButton::clicked, this, [this, cb]() { dev_->collectStop(cb); });
    connect(refreshBtn_, &QPushButton::clicked, this, &CollectPanel::refreshStatus);

    connect(dev, &DeviceClient::connectionChanged, this, [this](bool c, const QString&) {
        if (c) refreshStatus();
    });
}

void CollectPanel::refreshStatus()
{
    dev_->collectStatus([this](bool ok, bool collecting, const QString&) {
        if (ok)
            statusLabel_->setText(collecting
                ? QString::fromUtf8("采集中")
                : QString::fromUtf8("已停止"));
        else
            statusLabel_->setText(QString::fromUtf8("未知"));
    });
}

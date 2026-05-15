#include "StreamPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>

StreamPanel::StreamPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    // channel selection
    auto* grp = new QGroupBox(QString::fromUtf8("通道订阅"), this); // 通道订阅
    auto* vb = new QVBoxLayout(grp);

    chkRaw16_         = new QCheckBox("Raw16", this);
    chkSliceStitch16_ = new QCheckBox("SliceStitch16", this);

    vb->addWidget(chkRaw16_);
    vb->addWidget(chkSliceStitch16_);

    auto* btnRow = new QHBoxLayout();
    applyBtn_ = new QPushButton(QString::fromUtf8("应用订阅"), this); // 应用订阅
    applyBtn_->setProperty("primary", true);
    unsubBtn_ = new QPushButton(QString::fromUtf8("取消全部"), this); // 取消全部
    unsubBtn_->setProperty("danger", true);
    btnRow->addWidget(applyBtn_);
    btnRow->addWidget(unsubBtn_);
    vb->addLayout(btnRow);
    root->addWidget(grp);

    {
        auto* sgrp = new QGroupBox(QString::fromUtf8("状态"), this);
        auto* fl = new QFormLayout(sgrp);
        statusLabel_  = new QLabel("-", this);
        framesLabel_  = new QLabel("0", this);
        droppedLabel_ = new QLabel("0", this);
        fl->addRow(QString::fromUtf8("已订阅"), statusLabel_);
        fl->addRow(QString::fromUtf8("已发帧"), framesLabel_);
        fl->addRow(QString::fromUtf8("丢帧"), droppedLabel_);
        root->addWidget(sgrp);
    }

    root->addStretch();

    connect(applyBtn_, &QPushButton::clicked, this, &StreamPanel::onApply);
    connect(unsubBtn_, &QPushButton::clicked, this, &StreamPanel::onUnsubAll);

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(2000);
    connect(pollTimer_, &QTimer::timeout, this, &StreamPanel::refreshStatus);
    pollTimer_->start();
}

QStringList StreamPanel::selectedChannels() const
{
    QStringList chs;
    if (chkRaw16_->isChecked())          chs << "raw16";
    if (chkSliceStitch16_->isChecked())  chs << "slice_stitch16";
    return chs;
}

quint16 StreamPanel::udpPort() const { return udpPort_; }
void StreamPanel::setUdpPort(quint16 p) { udpPort_ = p; }

void StreamPanel::onApply()
{
    auto chs = selectedChannels();
    if (chs.isEmpty()) return;
    dev_->streamSubscribe(udpPort_, chs, [](bool, const QString&) {});
    emit subscribeRequested(udpPort_, chs);
}

void StreamPanel::onUnsubAll()
{
    dev_->streamUnsubscribeAll([](bool, const QString&) {});
}

void StreamPanel::refreshStatus()
{
    if (!dev_->isConnected()) return;
    dev_->streamStatus([this](bool ok, const nlohmann::json& data, const QString&) {
        if (!ok) return;
        auto chs = data.value("channels", std::vector<std::string>{});
        QStringList sl;
        for (auto& c : chs) sl << QString::fromStdString(c);
        statusLabel_->setText(sl.isEmpty() ? "-" : sl.join(", "));
        framesLabel_->setText(QString::number(data.value("frames_sent", (uint64_t)0)));
        droppedLabel_->setText(QString::number(data.value("frames_dropped", (uint64_t)0)));
    });
}

#include "DashboardPanel.h"
#include <QVariant>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFrame>

static QWidget* makeStatRow(const QString& key, QLabel*& valOut, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 3, 0, 3);
    lay->setSpacing(8);

    auto* keyLbl = new QLabel(key, row);
    keyLbl->setObjectName("dashStatKey");
    lay->addWidget(keyLbl);

    lay->addStretch();

    valOut = new QLabel("---", row);
    valOut->setObjectName("dashStatVal");
    lay->addWidget(valOut);

    return row;
}

static QGroupBox* makeCard(const QString& title, QWidget* parent)
{
    auto* grp = new QGroupBox(title, parent);
    grp->setObjectName("card");
    auto* lay = new QVBoxLayout(grp);
    lay->setContentsMargins(10, 14, 10, 10);
    lay->setSpacing(0);
    return grp;
}

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DashboardPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // Device overview card
    {
        auto* card = makeCard(QString::fromUtf8("设备概览"), this);
        auto* lay = qobject_cast<QVBoxLayout*>(card->layout());

        lay->addWidget(makeStatRow(QString::fromUtf8("TCP 地址"), dashTcp_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("UDP 端口"), dashUdp_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("连接状态"), dashConn_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("设备版本"), dashVer_, card));

        root->addWidget(card);
    }

    // Stream stats card
    {
        auto* card = makeCard(QString::fromUtf8("流统计"), this);
        auto* lay = qobject_cast<QVBoxLayout*>(card->layout());

        lay->addWidget(makeStatRow("Raw16 FPS", dashFpsRaw_, card));
        lay->addWidget(makeStatRow("SliceStitch16 FPS", dashFpsSlice_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("总接收帧数"), dashFrames_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("丢帧数"), dashDropped_, card));

        root->addWidget(card);
    }

    // Mirror info card
    {
        auto* card = makeCard(QString::fromUtf8("转镜信息"), this);
        auto* lay = qobject_cast<QVBoxLayout*>(card->layout());

        lay->addWidget(makeStatRow(QString::fromUtf8("当前角度"), dashAngle_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("运动状态"), dashMoving_, card));

        root->addWidget(card);
    }

    // System info card
    {
        auto* card = makeCard(QString::fromUtf8("系统信息"), this);
        auto* lay = qobject_cast<QVBoxLayout*>(card->layout());

        lay->addWidget(makeStatRow(QString::fromUtf8("应用版本"), dashVerSys_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("运行时间"), dashUptime_, card));
        lay->addWidget(makeStatRow(QString::fromUtf8("订阅通道"), dashChannels_, card));

        root->addWidget(card);
    }

    root->addStretch();

    // initial values
    dashVerSys_->setText("Spectra Pro v2.0");
    dashChannels_->setText("Raw16, SliceStitch16, SpectralPreview");
    dashMoving_->setText(QString::fromUtf8("已停止"));
    dashMoving_->setProperty("moving", false);
    dashMoving_->setStyleSheet("color: #26A641; font-weight: bold; font-size: 11pt;");
}

void DashboardPanel::setConnectionInfo(const QString& host, quint16 tcpPort, quint16,
                                        bool connected, const QString& version)
{
    dashTcp_->setText(QString("%1:%2").arg(host).arg(tcpPort));
    dashConn_->setText(connected
        ? QString::fromUtf8("已连接")
        : QString::fromUtf8("未连接"));
    dashConn_->setStyleSheet(connected
        ? "color: #26A641; font-weight: bold; font-size: 11pt;"
        : "color: #545D68; font-weight: bold; font-size: 11pt;");
    dashVer_->setText(version.isEmpty() ? "---" : version);
}

void DashboardPanel::setStreamStats(double fpsRaw, double fpsSlice, quint64 frames, quint64 dropped)
{
    dashFpsRaw_->setText(fpsRaw > 0 ? QString::number(fpsRaw, 'f', 1) : "--");
    dashFpsSlice_->setText(fpsSlice > 0 ? QString::number(fpsSlice, 'f', 1) : "--");
    dashFrames_->setText(QString::number(frames));
    dashDropped_->setText(QString::number(dropped));
    if (dropped > 0) dashDropped_->setStyleSheet("color: #E5484D; font-weight: bold; font-size: 11pt;");
}

void DashboardPanel::setMirrorInfo(double angle, bool moving)
{
    if (angle >= 0) {
        dashAngle_->setText(QString::number(angle, 'f', 3) + "°");
    }
    dashMoving_->setText(moving
        ? QString::fromUtf8("运动中")
        : QString::fromUtf8("已停止"));
    dashMoving_->setStyleSheet(moving
        ? "color: #D29922; font-weight: bold; font-size: 11pt;"
        : "color: #26A641; font-weight: bold; font-size: 11pt;");
}

void DashboardPanel::setUptime(const QString& uptime)
{
    dashUptime_->setText(uptime);
}

void DashboardPanel::setSubscribedChannels(const QString& channels)
{
    dashChannels_->setText(channels.isEmpty() ? "---" : channels);
}

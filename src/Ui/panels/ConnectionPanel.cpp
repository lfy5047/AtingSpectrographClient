#include "ConnectionPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDateTime>

ConnectionPanel::ConnectionPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    // server group
    {
        auto* grp = new QGroupBox(QString::fromUtf8("服务端连接"), this);
        auto* form = new QFormLayout(grp);
        form->setSpacing(6);

        hostEdit_ = new QLineEdit("192.168.10.128", this);
        tcpPortSpin_ = new QSpinBox(this);
        tcpPortSpin_->setRange(1, 65535);
        tcpPortSpin_->setValue(9000);
        udpPortSpin_ = new QSpinBox(this);
        udpPortSpin_->setRange(1, 65535);
        udpPortSpin_->setValue(1400);

        form->addRow(QString::fromUtf8("服务端 IP"), hostEdit_);
        form->addRow(QString::fromUtf8("TCP 端口"), tcpPortSpin_);
        form->addRow(QString::fromUtf8("本地 UDP 端口"), udpPortSpin_);

        root->addWidget(grp);
    }

    // buttons
    auto* btnRow = new QHBoxLayout();
    connectBtn_ = new QPushButton(QString::fromUtf8("连接"), this); // 连接
    connectBtn_->setProperty("primary", true);
    disconnectBtn_ = new QPushButton(QString::fromUtf8("断开"), this); // 断开
    disconnectBtn_->setProperty("danger", true);
    disconnectBtn_->setEnabled(false);
    pingBtn_ = new QPushButton("Ping", this);
    pingBtn_->setEnabled(false);

    btnRow->addWidget(connectBtn_);
    btnRow->addWidget(disconnectBtn_);
    btnRow->addWidget(pingBtn_);
    root->addLayout(btnRow);

    // status
    {
        auto* statusGrp = new QGroupBox(QString::fromUtf8("状态"), this);
        auto* sf = new QFormLayout(statusGrp);
        statusLabel_  = new QLabel(QString::fromUtf8("未连接"), this);
        versionLabel_ = new QLabel("-", this);
        pingLabel_    = new QLabel("-", this);
        pingLabel_->setProperty("secondary", true);
        sf->addRow(QString::fromUtf8("连接"), statusLabel_);
        sf->addRow(QString::fromUtf8("版本"), versionLabel_);
        sf->addRow("RTT", pingLabel_);
        root->addWidget(statusGrp);
    }

    root->addStretch();

    // signals
    connect(connectBtn_, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);
    connect(disconnectBtn_, &QPushButton::clicked, this, [this]() {
        dev_->disconnect();
    });
    connect(pingBtn_, &QPushButton::clicked, this, &ConnectionPanel::onPingClicked);
    connect(dev_, &DeviceClient::connectionChanged, this, &ConnectionPanel::onConnectionChanged);
}

QString ConnectionPanel::host() const { return hostEdit_->text().trimmed(); }
quint16 ConnectionPanel::tcpPort() const { return static_cast<quint16>(tcpPortSpin_->value()); }
quint16 ConnectionPanel::udpPort() const { return static_cast<quint16>(udpPortSpin_->value()); }

void ConnectionPanel::onConnectClicked()
{
    dev_->connectTo(host(), tcpPort());
}

void ConnectionPanel::onPingClicked()
{
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    dev_->ping([this, t0](bool ok, qint64, const QString& err) {
        if (ok) {
            qint64 rtt = QDateTime::currentMSecsSinceEpoch() - t0;
            pingLabel_->setText(QString("%1 ms").arg(rtt));
        } else {
            pingLabel_->setText(QString::fromUtf8("失败: ") + err); // 失败:
        }
    });
}

void ConnectionPanel::onConnectionChanged(bool connected, const QString& ip)
{
    connectBtn_->setEnabled(!connected);
    disconnectBtn_->setEnabled(connected);
    pingBtn_->setEnabled(connected);
    hostEdit_->setEnabled(!connected);
    tcpPortSpin_->setEnabled(!connected);

    if (connected) {
        statusLabel_->setText(QString::fromUtf8("已连接 ") + ip); // 已连接
        statusLabel_->setStyleSheet("color: #3FB950;");

        dev_->version([this](bool ok, int ver, const QString& name, const QString&) {
            if (ok) versionLabel_->setText(QString("%1 v%2").arg(name).arg(ver));
        });
    } else {
        statusLabel_->setText(QString::fromUtf8("未连接")); // 未连接
        statusLabel_->setStyleSheet("color: #E5484D;");
        versionLabel_->setText("-");
        pingLabel_->setText("-");
    }
}

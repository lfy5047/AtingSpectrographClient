#include "ConnectionPanel.h"
#include "DeviceClient.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>

ConnectionPanel::ConnectionPanel(DeviceClient* dev, QWidget* parent)
    : ConnectionPanel(dev, Full, parent)
{
}

ConnectionPanel::ConnectionPanel(DeviceClient* dev, Mode mode, QWidget* parent)
    : QWidget(parent)
    , dev_(dev)
    , mode_(mode)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* grp = new QGroupBox(QString::fromUtf8("服务端连接"), this);
    auto* form = new QFormLayout(grp);
    form->setSpacing(6);

    hostEdit_ = new QLineEdit("192.168.10.128", this);
    hostEdit_->setObjectName(QStringLiteral("connectionHostEdit"));
    tcpPortSpin_ = new QSpinBox(this);
    tcpPortSpin_->setObjectName(QStringLiteral("connectionTcpPortSpin"));
    tcpPortSpin_->setRange(1, 65535);
    tcpPortSpin_->setValue(9000);
    udpPortSpin_ = new QSpinBox(this);
    udpPortSpin_->setObjectName(QStringLiteral("connectionUdpPortSpin"));
    udpPortSpin_->setRange(1, 65535);
    udpPortSpin_->setValue(1400);

    form->addRow(QString::fromUtf8("服务端 IP"), hostEdit_);
    form->addRow(QString::fromUtf8("TCP 端口"), tcpPortSpin_);
    form->addRow(QString::fromUtf8("本地 UDP 端口"), udpPortSpin_);
    root->addWidget(grp);

    if (mode_ == Full || mode_ == SettingsWithToggle) {
        auto* btnRow = new QHBoxLayout();
        connectBtn_ = new QPushButton(QString::fromUtf8("连接"), this);
        connectBtn_->setObjectName(QStringLiteral("connectionConnectButton"));
        connectBtn_->setProperty("primary", true);

        btnRow->addWidget(connectBtn_);
        if (mode_ == Full) {
            disconnectBtn_ = new QPushButton(QString::fromUtf8("断开"), this);
            disconnectBtn_->setObjectName(QStringLiteral("connectionDisconnectButton"));
            disconnectBtn_->setProperty("danger", true);
            disconnectBtn_->setEnabled(false);
            pingBtn_ = new QPushButton("Ping", this);
            pingBtn_->setObjectName(QStringLiteral("connectionPingButton"));
            pingBtn_->setEnabled(false);

            btnRow->addWidget(disconnectBtn_);
            btnRow->addWidget(pingBtn_);
        }
        root->addLayout(btnRow);
    }

    if (mode_ == Full) {
        auto* statusGrp = new QGroupBox(QString::fromUtf8("状态"), this);
        auto* sf = new QFormLayout(statusGrp);
        statusLabel_ = new QLabel(QString::fromUtf8("未连接"), this);
        versionLabel_ = new QLabel("-", this);
        pingLabel_ = new QLabel("-", this);
        pingLabel_->setProperty("secondary", true);
        sf->addRow(QString::fromUtf8("连接"), statusLabel_);
        sf->addRow(QString::fromUtf8("版本"), versionLabel_);
        sf->addRow("RTT", pingLabel_);
        root->addWidget(statusGrp);
    }

    root->addStretch();

    loadSettings();

    connect(hostEdit_, &QLineEdit::textChanged, this, [this]() { saveSettings(); });
    connect(tcpPortSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    connect(udpPortSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });

    if (mode_ == Full || mode_ == SettingsWithToggle) {
        connect(connectBtn_, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);
        connect(dev_, &DeviceClient::connectionChanged, this, &ConnectionPanel::onConnectionChanged);
        onConnectionChanged(dev_->isConnected(), QString());
    }
    if (mode_ == Full) {
        connect(disconnectBtn_, &QPushButton::clicked, this, [this]() {
            dev_->disconnect();
        });
        connect(pingBtn_, &QPushButton::clicked, this, &ConnectionPanel::onPingClicked);
    }
}

QString ConnectionPanel::host() const { return hostEdit_->text().trimmed(); }
quint16 ConnectionPanel::tcpPort() const { return static_cast<quint16>(tcpPortSpin_->value()); }
quint16 ConnectionPanel::udpPort() const { return static_cast<quint16>(udpPortSpin_->value()); }

void ConnectionPanel::loadSettings()
{
    QSettings s;
    const QString p = QStringLiteral("panels/connection/");
    hostEdit_->setText(s.value(p + QStringLiteral("host"), hostEdit_->text()).toString());
    tcpPortSpin_->setValue(s.value(p + QStringLiteral("tcpPort"), tcpPortSpin_->value()).toInt());
    udpPortSpin_->setValue(s.value(p + QStringLiteral("udpPort"), udpPortSpin_->value()).toInt());
}

void ConnectionPanel::saveSettings() const
{
    QSettings s;
    const QString p = QStringLiteral("panels/connection/");
    s.setValue(p + QStringLiteral("host"), hostEdit_->text().trimmed());
    s.setValue(p + QStringLiteral("tcpPort"), tcpPortSpin_->value());
    s.setValue(p + QStringLiteral("udpPort"), udpPortSpin_->value());
}

void ConnectionPanel::onConnectClicked()
{
    if (mode_ == SettingsWithToggle) {
        emit connectToggleRequested();
        return;
    }
    dev_->connectTo(host(), tcpPort());
}

void ConnectionPanel::onPingClicked()
{
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();
    dev_->systemApi()->ping(this, [this, t0](bool ok, qint64, const QString& err) {
        if (ok) {
            qint64 rtt = QDateTime::currentMSecsSinceEpoch() - t0;
            pingLabel_->setText(QString("%1 ms").arg(rtt));
        } else {
            pingLabel_->setText(QString::fromUtf8("失败: ") + err);
        }
    });
}

void ConnectionPanel::onConnectionChanged(bool connected, const QString& ip)
{
    if (mode_ != Full && mode_ != SettingsWithToggle) return;

    if (mode_ == Full) {
        connectBtn_->setEnabled(!connected);
        disconnectBtn_->setEnabled(connected);
        pingBtn_->setEnabled(connected);
    } else {
        connectBtn_->setText(connected ? QString::fromUtf8("断开") : QString::fromUtf8("连接"));
        connectBtn_->setProperty("danger", connected);
        connectBtn_->setProperty("primary", !connected);
        connectBtn_->style()->polish(connectBtn_);
    }
    hostEdit_->setEnabled(!connected);
    tcpPortSpin_->setEnabled(!connected);

    if (mode_ == SettingsWithToggle) return;

    if (connected) {
        statusLabel_->setText(QString::fromUtf8("已连接 ") + ip);
        statusLabel_->setProperty("connState", "ok");

        dev_->systemApi()->version(this, [this](bool ok, int ver, const QString& name, const QString&) {
            if (ok) versionLabel_->setText(QString("%1 v%2").arg(name).arg(ver));
        });
    } else {
        statusLabel_->setText(QString::fromUtf8("未连接"));
        statusLabel_->setProperty("connState", "err");
        versionLabel_->setText("-");
        pingLabel_->setText("-");
    }
    statusLabel_->style()->polish(statusLabel_);
}

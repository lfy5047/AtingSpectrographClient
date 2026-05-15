#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

class DeviceClient;

class ConnectionPanel : public QWidget {
    Q_OBJECT
public:
    explicit ConnectionPanel(DeviceClient* dev, QWidget* parent = nullptr);

    QString host() const;
    quint16 tcpPort() const;
    quint16 udpPort() const;

private slots:
    void onConnectClicked();
    void onPingClicked();
    void onConnectionChanged(bool connected, const QString& ip);

private:
    DeviceClient* dev_;

    QLineEdit*   hostEdit_;
    QSpinBox*    tcpPortSpin_;
    QSpinBox*    udpPortSpin_;
    QPushButton* connectBtn_;
    QPushButton* disconnectBtn_;
    QPushButton* pingBtn_;
    QLabel*      statusLabel_;
    QLabel*      versionLabel_;
    QLabel*      pingLabel_;
};

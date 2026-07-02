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
    enum Mode {
        Full,
        SettingsOnly,
        SettingsWithToggle,
    };

    explicit ConnectionPanel(DeviceClient* dev, QWidget* parent = nullptr);
    ConnectionPanel(DeviceClient* dev, Mode mode, QWidget* parent = nullptr);

    QString host() const;
    quint16 tcpPort() const;
    quint16 udpPort() const;

signals:
    void connectToggleRequested();

private slots:
    void onConnectClicked();
    void onPingClicked();
    void onConnectionChanged(bool connected, const QString& ip);

private:
    void loadSettings();
    void saveSettings() const;

    DeviceClient* dev_;
    Mode mode_ = Full;

    QLineEdit*   hostEdit_;
    QSpinBox*    tcpPortSpin_;
    QSpinBox*    udpPortSpin_;
    QPushButton* connectBtn_ = nullptr;
    QPushButton* disconnectBtn_ = nullptr;
    QPushButton* pingBtn_ = nullptr;
    QLabel*      statusLabel_ = nullptr;
    QLabel*      versionLabel_ = nullptr;
    QLabel*      pingLabel_ = nullptr;
};

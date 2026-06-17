#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

class DeviceClient;

class CameraPanel : public QWidget {
    Q_OBJECT
public:
    explicit CameraPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void refreshResolution();
    void onApplyResolution();
    void refreshDevices();
    void onDeviceChanged(int index);

private:
    void loadSettings();
    void saveSettings() const;
    void reloadDeviceUi();
    void applySelectedMac(const QString& mac);

    DeviceClient* dev_;
    QComboBox*   deviceCombo_ = nullptr;
    QPushButton* deviceRefreshBtn_ = nullptr;
    bool         deviceRefreshing_ = false;
    QString      deviceSelectedMac_;

    QLabel*      curResLabel_;
    QSpinBox*    widthSpin_;
    QSpinBox*    heightSpin_;
    QPushButton* applyBtn_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* refreshBtn_;
};

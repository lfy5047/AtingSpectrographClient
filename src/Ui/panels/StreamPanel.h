#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

class DeviceClient;

class StreamPanel : public QWidget {
    Q_OBJECT
public:
    explicit StreamPanel(DeviceClient* dev, QWidget* parent = nullptr);

    QStringList selectedChannels() const;
    quint16 udpPort() const;
    void setUdpPort(quint16 p);

signals:
    void subscribeRequested(quint16 port, QStringList channels);

private slots:
    void onApply();
    void onUnsubAll();
    void refreshStatus();

private:
    DeviceClient* dev_;

    QCheckBox* chkPreview8_       = nullptr;
    QCheckBox* chkRaw16_          = nullptr;
    QCheckBox* chkSliceStitch16_  = nullptr;
    QCheckBox* chkRegionStitch16_ = nullptr;

    QPushButton* applyBtn_   = nullptr;
    QPushButton* unsubBtn_   = nullptr;

    QLabel* statusLabel_     = nullptr;
    QLabel* framesLabel_     = nullptr;
    QLabel* droppedLabel_    = nullptr;

    QTimer* pollTimer_       = nullptr;
    quint16 udpPort_         = 9001;
};

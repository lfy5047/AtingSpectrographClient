#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class DeviceClient;

class CollectPanel : public QWidget {
    Q_OBJECT
public:
    explicit CollectPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void refreshStatus();

private:
    DeviceClient* dev_;
    QLabel*      statusLabel_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* refreshBtn_;
};

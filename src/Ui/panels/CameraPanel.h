#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

class DeviceClient;

class CameraPanel : public QWidget {
    Q_OBJECT
public:
    explicit CameraPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void refreshResolution();
    void onApplyResolution();

private:
    DeviceClient* dev_;
    QLabel*      curResLabel_;
    QSpinBox*    widthSpin_;
    QSpinBox*    heightSpin_;
    QPushButton* applyBtn_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* refreshBtn_;
};

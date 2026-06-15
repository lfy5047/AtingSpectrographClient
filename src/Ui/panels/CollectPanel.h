#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class DeviceClient;
class QSpinBox;
struct CollectOversamplingInfo;

class CollectPanel : public QWidget {
    Q_OBJECT
public:
    explicit CollectPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void refreshStatus();
    void applyOversampling();

private:
    void updateOversamplingUi(const CollectOversamplingInfo& info);

    DeviceClient* dev_;
    QLabel*      statusLabel_;
    QLabel*      effectiveSSpeedLabel_;
    QLabel*      effectiveFSpeedLabel_;
    QSpinBox*    oversampleFactorSpin_;
    QPushButton* applyOversamplingBtn_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* refreshBtn_;
};

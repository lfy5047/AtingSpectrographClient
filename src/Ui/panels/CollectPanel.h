#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class DeviceClient;
class QSpinBox;
struct CollectOversamplingInfo;
struct CollectGateConfig;

class CollectPanel : public QWidget {
    Q_OBJECT
public:
    explicit CollectPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void refreshStatus();
    void applyOversampling();
    void refreshGateConfig();
    void applyGateConfig();

private:
    void loadSettings();
    void saveSettings() const;
    void updateOversamplingUi(const CollectOversamplingInfo& info);
    void updateGateConfigUi(const CollectGateConfig& config);

    DeviceClient* dev_;
    QLabel*      statusLabel_;
    QLabel*      effectiveSSpeedLabel_;
    QLabel*      effectiveFSpeedLabel_;
    QLabel*      gateCollectingLabel_;
    QLabel*      gatePendingLabel_;
    QSpinBox*    oversampleFactorSpin_;
    QSpinBox*    discardFrontMsSpin_;
    QSpinBox*    discardBackMsSpin_;
    QSpinBox*    forwardOffsetFramesSpin_;
    QSpinBox*    reverseOffsetFramesSpin_;
    QPushButton* applyOversamplingBtn_;
    QPushButton* refreshGateConfigBtn_;
    QPushButton* applyGateConfigBtn_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* refreshBtn_;
};

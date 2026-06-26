#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class DeviceClient;
class QSpinBox;
class QTimer;
struct CollectOversamplingInfo;
struct CollectGateConfig;
struct BackgroundCalibrationStatus;

class CollectPanel : public QWidget {
    Q_OBJECT
public:
    explicit CollectPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void refreshStatus();
    void applyOversampling();
    void refreshGateConfig();
    void applyGateConfig();
    void startBackgroundCalibration();
    void pollBackgroundCalibrationStatus();
    void finishBackgroundCalibration(const BackgroundCalibrationStatus& status);
    void setBackgroundCalibrationStage(const QString& stage, const QString& error = QString());

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
    QLabel* backgroundCalibrationStatusLabel_ = nullptr;
    QPushButton* backgroundCalibrationBtn_ = nullptr;
    QTimer* backgroundCalibrationTimer_ = nullptr;
};

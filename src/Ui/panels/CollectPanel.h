#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>

class DeviceClient;
class QCheckBox;
class QSpinBox;
struct CollectOversamplingInfo;
struct CollectGateConfig;

class CollectPanel : public QWidget {
    Q_OBJECT
public:
    enum Mode {
        Full,
        OperationOnly,
    };

    explicit CollectPanel(DeviceClient* dev, QWidget* parent = nullptr);
    CollectPanel(DeviceClient* dev, Mode mode, QWidget* parent = nullptr);

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
    QLabel*      statusLabel_ = nullptr;
    QLabel*      effectiveSSpeedLabel_ = nullptr;
    QLabel*      effectiveFSpeedLabel_ = nullptr;
    QLabel*      gateCollectingLabel_ = nullptr;
    QLabel*      gatePendingLabel_ = nullptr;
    QSpinBox*    oversampleFactorSpin_ = nullptr;
    QSpinBox*    discardFrontMsSpin_ = nullptr;
    QSpinBox*    discardBackMsSpin_ = nullptr;
    QSpinBox*    forwardOffsetFramesSpin_ = nullptr;
    QSpinBox*    reverseOffsetFramesSpin_ = nullptr;
    QCheckBox*   staticCollectModeCheck_ = nullptr;
    QPushButton* applyOversamplingBtn_ = nullptr;
    QPushButton* refreshGateConfigBtn_ = nullptr;
    QPushButton* applyGateConfigBtn_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
};

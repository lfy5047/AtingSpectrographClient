#pragma once

#include <QWidget>

#include "TempControlService.h"
#include "json.hpp"

class DeviceClient;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;

class TempControlPanel : public QWidget {
public:
    explicit TempControlPanel(DeviceClient* dev, QWidget* parent = nullptr);

private:
    void setupUi();
    void loadSettings();
    void saveSettings() const;
    void refreshStatus();
    void setStatusUnavailable(const QString& reason);
    void updateStatusUi(const TempControlStatus& status);
    void setResult(const QString& text);
    void runJsonAction(const QString& label, const std::function<void(JsonCallback)>& action);
    QString selectedKey() const;
    QString selectedKeyName() const;
    QString advancedValue() const;
    static QString jsonToText(const nlohmann::json& data);

    DeviceClient* dev_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    bool statusPending_ = false;

    QLabel* actualTemperatureLabel_ = nullptr;
    QLabel* adjustTemperatureLabel_ = nullptr;
    QLabel* actualVoltageLabel_ = nullptr;
    QLabel* switchLabel_ = nullptr;
    QLabel* outputEnabledLabel_ = nullptr;
    QLabel* resultLabel_ = nullptr;

    QDoubleSpinBox* targetTemperatureSpin_ = nullptr;
    QDoubleSpinBox* maxTemperatureSpin_ = nullptr;
    QDoubleSpinBox* maxVoltageSpin_ = nullptr;
    QPushButton* setTargetButton_ = nullptr;
    QPushButton* saveTargetButton_ = nullptr;
    QPushButton* setMaxTemperatureButton_ = nullptr;
    QPushButton* setMaxVoltageButton_ = nullptr;
    QPushButton* switchOnButton_ = nullptr;
    QPushButton* switchOffButton_ = nullptr;

    QComboBox* keyCombo_ = nullptr;
    QLineEdit* advancedValueEdit_ = nullptr;
    QPushButton* queryKeyButton_ = nullptr;
    QPushButton* setKeyButton_ = nullptr;
    QPushButton* saveKeyButton_ = nullptr;
};

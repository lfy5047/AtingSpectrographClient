#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include "json.hpp"

class DeviceClient;

class IrPanel : public QWidget {
    Q_OBJECT
public:
    explicit IrPanel(DeviceClient* dev, QWidget* parent = nullptr);

private:
    void setupImageParams(class QVBoxLayout* root);
    void setupIntegration(class QVBoxLayout* root);
    void setupImageDisplay(class QVBoxLayout* root);
    void setupFilters(class QVBoxLayout* root);
    void setupFlipSync(class QVBoxLayout* root);
    void setupModeControl(class QVBoxLayout* root);
    void setupQueries(class QVBoxLayout* root);
    void setupMaintenance(class QVBoxLayout* root);
    void setupBadPixel(class QVBoxLayout* root);

    void loadSettings();
    void saveSettings() const;
    void connectSettingSignals();

    void setActionStatus(const QString& text);
    void updateReadoutLabel(QLabel* label, const QString& title, bool ok, const nlohmann::json& data,
                            const QString& err, const QString& unit = QString());
    static QString jsonValueToText(const nlohmann::json& value);
    static QString bytesToHex(const nlohmann::json& arr);

    DeviceClient* dev_;

    // 亮度 / 对比度 / DDE / AB 模式
    QSpinBox*  brightSpin_ = nullptr;
    QSpinBox*  contrastSpin_ = nullptr;
    QSpinBox*  ddeSpin_ = nullptr;
    QComboBox* abModeCombo_ = nullptr;

    // 积分时间
    QSpinBox*  integSpin_ = nullptr;
    QComboBox* integModeCombo_ = nullptr;
    QComboBox* gearModeCombo_ = nullptr;
    QComboBox* gearSelectCombo_ = nullptr;

    // 图像显示
    QComboBox* imageTypeCombo_ = nullptr;
    QComboBox* testPatternCombo_ = nullptr;
    QComboBox* colorModeCombo_ = nullptr;
    QComboBox* badPixelDispCombo_ = nullptr;

    // 滤波
    QCheckBox* tempFilterChk_ = nullptr;
    QSpinBox*  tempFilterCoeffSpin_ = nullptr;
    QCheckBox* medianFilterChk_ = nullptr;
    QSpinBox*  medianFilterCoeffSpin_ = nullptr;

    // 翻转与同步
    QComboBox* flipHCombo_ = nullptr;
    QComboBox* flipVCombo_ = nullptr;
    QComboBox* extSyncCombo_ = nullptr;

    // 模式控制
    QComboBox* standbyCombo_ = nullptr;
    QComboBox* autoCalibCombo_ = nullptr;

    // 维护与校正
    QComboBox* maintUnlockCombo_ = nullptr;
    QComboBox* maintExecNameCombo_ = nullptr;
    QSpinBox*  maintExecValueSpin_ = nullptr;
    QComboBox* clearKCombo_ = nullptr;
    QComboBox* clearBCombo_ = nullptr;

    // 坏元管理
    QComboBox* badPixelSearchCombo_ = nullptr;
    QSpinBox*  badPixelPosSpin_[4] = {nullptr, nullptr, nullptr, nullptr};

    // 动作状态标签。读取结果使用各自就近的 QLabel。
    QLabel*    actionStatusLabel_ = nullptr;
};

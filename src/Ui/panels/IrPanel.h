#pragma once

#include <QWidget>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
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
    void setupRawCmd(class QVBoxLayout* root);

    void showResult(const QString& title, bool ok, const nlohmann::json& data, const QString& err);
    static QString bytesToHex(const nlohmann::json& arr);

    DeviceClient* dev_;

    // 亮度 / 对比度 / DDE / AB 模式
    QSlider*   brightSlider_ = nullptr;
    QSpinBox*  brightSpin_ = nullptr;
    QSlider*   contrastSlider_ = nullptr;
    QSpinBox*  contrastSpin_ = nullptr;
    QSlider*   ddeSlider_ = nullptr;
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

    // 原始命令
    QLineEdit* rawCmdEdit_ = nullptr;
    QLineEdit* rawDataEdit_ = nullptr;
    QSpinBox*  rawLenSpin_ = nullptr;
    QLabel*    rawResultLabel_ = nullptr;

    // 共享结果标签
    QLabel*    resultLabel_ = nullptr;
};

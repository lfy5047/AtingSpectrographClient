#pragma once

#include <QWidget>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QStackedWidget>
#include <QString>
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
    void setupLegacyPage(class QVBoxLayout* root);
    void setupCi05Page(class QVBoxLayout* root);

    void queryCurrentModel();
    bool isCi05Model() const;
    void updateModelUi();

    void loadSettings();
    void saveSettings() const;
    void connectSettingSignals();

    void setActionStatus(const QString& text);
    void updateReadoutLabel(QLabel* label, const QString& title, bool ok, const nlohmann::json& data,
                            const QString& err, const QString& unit = QString());
    static QString jsonValueToText(const nlohmann::json& value);
    static QString bytesToHex(const nlohmann::json& arr);

    DeviceClient* dev_;
    QString currentModel_ = QStringLiteral("legacy");
    QLabel* currentModelLabel_ = nullptr;
    QStackedWidget* coreModeStack_ = nullptr;
    QWidget* legacyPage_ = nullptr;
    QWidget* ci05Page_ = nullptr;

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

    QSpinBox*  ci05BrightnessSpin_ = nullptr;
    QSpinBox*  ci05ContrastSpin_ = nullptr;
    QSpinBox*  ci05OverallBrightnessSpin_ = nullptr;
    QSpinBox*  ci05OverallContrastSpin_ = nullptr;
    QSpinBox*  ci05SharpnessSpin_ = nullptr;
    QComboBox* ci05EzoomCombo_ = nullptr;
    QComboBox* ci05FreezeCombo_ = nullptr;
    QComboBox* ci05MirrorCombo_ = nullptr;
    QComboBox* ci05PaletteCombo_ = nullptr;
    QComboBox* ci05AgcCombo_ = nullptr;
    QComboBox* ci05FovCombo_ = nullptr;
    QSpinBox*  ci05FocusSpeedSpin_ = nullptr;
    QSpinBox*  ci05ZoomSpeedSpin_ = nullptr;
    QSpinBox*  ci05IntegrationMsSpin_ = nullptr;
    QSpinBox*  ci05IntegrationMcSpin_ = nullptr;
    QSpinBox*  ci05FrameRateSpin_ = nullptr;
    QComboBox* ci05IntegrationGearCombo_ = nullptr;
    QComboBox* ci05IntegrationAutoCombo_ = nullptr;
    QComboBox* ci05BackgroundGearCombo_ = nullptr;
    QComboBox* ci05BackgroundAutoCombo_ = nullptr;
    QComboBox* ci05SyncModeCombo_ = nullptr;
    QComboBox* ci05Y8LevelCombo_ = nullptr;
    QComboBox* ci05BootCompensationModeCombo_ = nullptr;
    QComboBox* ci05GearSwitchCompensationModeCombo_ = nullptr;
    QComboBox* ci05VideoSourceCombo_ = nullptr;
    QComboBox* ci05ParamLineCombo_ = nullptr;
    QComboBox* ci05DigitalFormatCombo_ = nullptr;
    QComboBox* ci05TestPatternCombo_ = nullptr;
    QComboBox* ci05ImageModeCombo_ = nullptr;
    QComboBox* ci05StatusOutputModeCombo_ = nullptr;
    QSpinBox*  ci05TmodFilterSpin_ = nullptr;
    QSpinBox*  ci05NtmFilterSpin_ = nullptr;
    QComboBox* ci05VerticalStripeRemovalCombo_ = nullptr;
    QLabel*    ci05ActionStatusLabel_ = nullptr;
};

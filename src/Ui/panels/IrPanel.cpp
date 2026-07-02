#include "IrPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QSettings>
#include <QSizePolicy>
#include <string>

#include "PanelSettings.h"

namespace {

QGridLayout* makeGrid2Col(std::initializer_list<QWidget*> widgets)
{
    auto* grid = new QGridLayout();
    int idx = 0;
    for (auto* w : widgets) {
        grid->addWidget(w, idx / 2, idx % 2);
        ++idx;
    }
    return grid;
}

void configureInlineField(QWidget* field)
{
    field->setMinimumWidth(0);
    field->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

void configureInlineButton(QPushButton* button)
{
    button->setProperty("small", true);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void configureCompactSpin(QSpinBox* spin, int width = 64)
{
    spin->setMinimumWidth(width);
    spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QWidget* makeValueButtonRow(QWidget* value, QPushButton* button, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* row = new QHBoxLayout(container);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    configureInlineField(value);
    configureInlineButton(button);
    row->addWidget(value, 1);
    row->addWidget(button);
    return container;
}

QLabel* makeReadoutLabel(const QString& title, QWidget* parent)
{
    auto* label = new QLabel(title + ": -", parent);
    // label->setProperty("readoutSm", true);
    label->setMinimumWidth(140);
    label->setWordWrap(true);
    return label;
}

QWidget* makeReadoutButtonRow(QLabel* readout, QPushButton* button, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* row = new QHBoxLayout(container);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    readout->setMinimumWidth(0);
    readout->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    configureInlineButton(button);
    row->addWidget(readout, 1);
    row->addWidget(button);
    return container;
}

} // namespace

IrPanel::IrPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* modeRow = new QHBoxLayout();
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(6);
    currentModelLabel_ = makeReadoutLabel(QString::fromUtf8("当前机芯"), this);
    currentModelLabel_->setObjectName(QStringLiteral("irCurrentModelLabel"));
    auto* queryModelBtn = new QPushButton(QString::fromUtf8("查询机芯"), this);
    queryModelBtn->setObjectName(QStringLiteral("irQueryCurrentModelButton"));
    configureInlineButton(queryModelBtn);
    modeRow->addWidget(currentModelLabel_, 1);
    modeRow->addWidget(queryModelBtn);
    root->addLayout(modeRow);

    coreModeStack_ = new QStackedWidget(this);
    coreModeStack_->setObjectName(QStringLiteral("irCoreModeStack"));

    legacyPage_ = new QWidget(this);
    legacyPage_->setObjectName(QStringLiteral("irLegacyPage"));
    auto* legacyRoot = new QVBoxLayout(legacyPage_);
    legacyRoot->setContentsMargins(0, 0, 0, 0);
    setupLegacyPage(legacyRoot);

    ci05Page_ = new QWidget(this);
    ci05Page_->setObjectName(QStringLiteral("irCi05Page"));
    auto* ci05Root = new QVBoxLayout(ci05Page_);
    ci05Root->setContentsMargins(0, 0, 0, 0);
    setupCi05Page(ci05Root);

    coreModeStack_->addWidget(legacyPage_);
    coreModeStack_->addWidget(ci05Page_);
    root->addWidget(coreModeStack_);

    loadSettings();
    connectSettingSignals();
    updateModelUi();

    connect(queryModelBtn, &QPushButton::clicked, this, [this]() {
        queryCurrentModel();
    });

    if (dev_) {
        connect(dev_, &DeviceClient::connectionChanged, this, [this](bool connected, const QString&) {
            if (connected) queryCurrentModel();
        });
        if (dev_->isConnected()) queryCurrentModel();
    }
}

void IrPanel::setupLegacyPage(QVBoxLayout* root)
{
    auto* commonGroup = new QGroupBox(QString::fromUtf8("常用参数"), this);
    commonGroup->setObjectName(QStringLiteral("irLegacyCommonGroup"));
    auto* commonRoot = new QVBoxLayout(commonGroup);
    auto* backgroundCorrectionBtn = new QPushButton(QString::fromUtf8("背景矫正"), this);
    backgroundCorrectionBtn->setObjectName(QStringLiteral("irLegacyBackgroundCorrectionButton"));
    commonRoot->addWidget(backgroundCorrectionBtn);
    setupIntegration(commonRoot);
    setupImageDisplay(commonRoot);
    setupFlipSync(commonRoot);
    setupModeControl(commonRoot);
    root->addWidget(commonGroup);

    auto* advancedGroup = new QGroupBox(QString::fromUtf8("高级设置"), this);
    advancedGroup->setObjectName(QStringLiteral("detectorAdvancedGroup"));
    advancedGroup->setCheckable(true);
    advancedGroup->setChecked(false);
    auto* advancedLayout = new QVBoxLayout(advancedGroup);
    auto* advancedContent = new QWidget(advancedGroup);
    auto* advancedRoot = new QVBoxLayout(advancedContent);
    advancedRoot->setContentsMargins(0, 0, 0, 0);
    setupImageParams(advancedRoot);
    setupFilters(advancedRoot);
    setupQueries(advancedRoot);
    setupMaintenance(advancedRoot);
    setupBadPixel(advancedRoot);
    advancedLayout->addWidget(advancedContent);
    advancedContent->setVisible(false);
    connect(advancedGroup, &QGroupBox::toggled, advancedContent, &QWidget::setVisible);
    root->addWidget(advancedGroup);
    root->addStretch();

    connect(backgroundCorrectionBtn, &QPushButton::clicked, this, [this]() {
        dev_->ir()->triggerCalibration(this, [this](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, "IR", err);
            setActionStatus(ok ? "OK" : err);
        });
    });
}

void IrPanel::setupCi05Page(QVBoxLayout* root)
{
    auto writeCb = [this](bool ok, const QString& err) {
        const QString text = ok ? QStringLiteral("OK") : err;
        if (ci05ActionStatusLabel_)
            ci05ActionStatusLabel_->setText(text);
        if (!ok)
            QMessageBox::warning(this, "CI05", err);
    };

    auto makeSpin = [this](int min, int max, const QString& name) {
        auto* spin = new QSpinBox(this);
        spin->setRange(min, max);
        spin->setObjectName(name);
        return spin;
    };

    auto makeCombo = [this](const QString& name) {
        auto* combo = new QComboBox(this);
        combo->setObjectName(name);
        return combo;
    };
    auto addRangeItems = [](QComboBox* combo, const QString& prefix, int min, int max) {
        for (int i = min; i <= max; ++i)
            combo->addItem(prefix + QString::number(i), i);
    };
    auto addOffOnItems = [](QComboBox* combo) {
        combo->addItem(QString::fromUtf8("关"), 0);
        combo->addItem(QString::fromUtf8("开"), 1);
    };
    auto comboValue = [](QComboBox* combo) {
        return static_cast<quint8>(combo->currentData().toInt());
    };

    auto* commonGroup = new QGroupBox(QString::fromUtf8("常用参数"), this);
    commonGroup->setObjectName(QStringLiteral("irCi05CommonGroup"));
    auto* commonRoot = new QVBoxLayout(commonGroup);

    auto* advancedGroup = new QGroupBox(QString::fromUtf8("高级设置"), this);
    advancedGroup->setObjectName(QStringLiteral("irCi05AdvancedGroup"));
    advancedGroup->setCheckable(true);
    advancedGroup->setChecked(false);
    auto* advancedLayout = new QVBoxLayout(advancedGroup);
    auto* advancedContent = new QWidget(advancedGroup);
    auto* advancedRoot = new QVBoxLayout(advancedContent);
    advancedRoot->setContentsMargins(0, 0, 0, 0);
    advancedLayout->addWidget(advancedContent);
    advancedContent->setVisible(false);
    connect(advancedGroup, &QGroupBox::toggled, advancedContent, &QWidget::setVisible);

    auto* commonCompensationGroup = new QGroupBox(QString::fromUtf8("CI05 补偿"), this);
    auto* commonCompensationLayout = new QVBoxLayout(commonCompensationGroup);
    auto* shutterCompensation = new QPushButton(QString::fromUtf8("快门补偿"), this);
    shutterCompensation->setObjectName(QStringLiteral("irCi05TriggerShutterCompensationButton"));
    auto* sceneCompensation = new QPushButton(QString::fromUtf8("场景补偿"), this);
    sceneCompensation->setObjectName(QStringLiteral("irCi05TriggerSceneCompensationButton"));
    commonCompensationLayout->addLayout(makeGrid2Col({shutterCompensation, sceneCompensation}));
    auto* sceneHint = new QLabel(QString::fromUtf8("场景补偿前请确保画面为均匀场景。"), this);
    sceneHint->setWordWrap(true);
    sceneHint->setProperty("secondary", true);
    commonCompensationLayout->addWidget(sceneHint);
    commonRoot->addWidget(commonCompensationGroup);

    auto* commonIntegrationGroup = new QGroupBox(QString::fromUtf8("CI05 积分时间设置"), this);
    auto* commonIntegrationForm = new QFormLayout(commonIntegrationGroup);
    ci05IntegrationMsSpin_ = makeSpin(0, 65535, QStringLiteral("irCi05IntegrationMsSpin"));
    auto* applyIntegration = new QPushButton(QString::fromUtf8("设积分 0.1ms"), this);
    commonIntegrationForm->addRow(QString::fromUtf8("积分时间"), makeValueButtonRow(ci05IntegrationMsSpin_, applyIntegration, this));
    auto* integrationIncrease = new QPushButton(QString::fromUtf8("积分 +0.1ms"), this);
    auto* integrationDecrease = new QPushButton(QString::fromUtf8("积分 -0.1ms"), this);
    commonIntegrationForm->addRow(QString::fromUtf8("微调"), makeReadoutButtonRow(new QLabel(QStringLiteral("-"), this), integrationIncrease, this));
    commonIntegrationForm->addRow(QString(), makeReadoutButtonRow(new QLabel(QStringLiteral("-"), this), integrationDecrease, this));
    commonRoot->addWidget(commonIntegrationGroup);
    root->addWidget(commonGroup);

    auto* lensGroup = new QGroupBox(QString::fromUtf8("CI05 镜头控制"), this);
    auto* lensLayout = new QVBoxLayout(lensGroup);
    auto* focusPos = new QPushButton(QString::fromUtf8("调焦 +"), this);
    auto* focusNeg = new QPushButton(QString::fromUtf8("调焦 -"), this);
    auto* focusStepPos = new QPushButton(QString::fromUtf8("单步调焦 +"), this);
    auto* focusStepNeg = new QPushButton(QString::fromUtf8("单步调焦 -"), this);
    auto* zoomPos = new QPushButton(QString::fromUtf8("变倍 +"), this);
    auto* zoomNeg = new QPushButton(QString::fromUtf8("变倍 -"), this);
    auto* zoomStepPos = new QPushButton(QString::fromUtf8("单步变倍 +"), this);
    auto* zoomStepNeg = new QPushButton(QString::fromUtf8("单步变倍 -"), this);
    auto* autoFocus = new QPushButton(QString::fromUtf8("自动聚焦"), this);
    lensLayout->addLayout(makeGrid2Col({focusPos, focusNeg, focusStepPos, focusStepNeg,
                                        zoomPos, zoomNeg, zoomStepPos, zoomStepNeg,
                                        autoFocus}));

    auto* lensForm = new QFormLayout();
    ci05FovCombo_ = new QComboBox(this);
    ci05FovCombo_->setObjectName(QStringLiteral("irCi05FovCombo"));
    ci05FovCombo_->addItem(QString::fromUtf8("宽视场"), 0);
    ci05FovCombo_->addItem(QString::fromUtf8("中视场"), 1);
    ci05FovCombo_->addItem(QString::fromUtf8("窄视场"), 2);
    auto* applyFov = new QPushButton(QString::fromUtf8("设视场"), this);
    lensForm->addRow(QString::fromUtf8("视场"), makeValueButtonRow(ci05FovCombo_, applyFov, this));

    ci05FocusSpeedSpin_ = makeSpin(0, 10, QStringLiteral("irCi05FocusSpeedSpin"));
    auto* applyFocusSpeed = new QPushButton(QString::fromUtf8("设调焦速度"), this);
    lensForm->addRow(QString::fromUtf8("调焦速度"), makeValueButtonRow(ci05FocusSpeedSpin_, applyFocusSpeed, this));

    ci05ZoomSpeedSpin_ = makeSpin(0, 10, QStringLiteral("irCi05ZoomSpeedSpin"));
    auto* applyZoomSpeed = new QPushButton(QString::fromUtf8("设变倍速度"), this);
    lensForm->addRow(QString::fromUtf8("变倍速度"), makeValueButtonRow(ci05ZoomSpeedSpin_, applyZoomSpeed, this));
    auto* shutterOpen = new QPushButton(QString::fromUtf8("快门打开"), this);
    auto* shutterClose = new QPushButton(QString::fromUtf8("快门关闭"), this);
    lensForm->addRow(QString::fromUtf8("快门"), makeReadoutButtonRow(new QLabel(QStringLiteral("-"), this), shutterOpen, this));
    lensForm->addRow(QString(), makeReadoutButtonRow(new QLabel(QStringLiteral("-"), this), shutterClose, this));
    auto* presetSpin = makeSpin(0, 255, QStringLiteral("irCi05PresetSpin"));
    auto* callPreset = new QPushButton(QString::fromUtf8("调用预置位"), this);
    auto* setPreset = new QPushButton(QString::fromUtf8("设置预置位"), this);
    lensForm->addRow(QString::fromUtf8("预置位"), makeValueButtonRow(presetSpin, callPreset, this));
    lensForm->addRow(QString(), makeReadoutButtonRow(new QLabel(QStringLiteral("-"), this), setPreset, this));
    auto* focalLengthSpin = makeSpin(0, 65535, QStringLiteral("irCi05FocalLengthSpin"));
    auto* applyFocalLength = new QPushButton(QString::fromUtf8("设焦距 0.1mm"), this);
    lensForm->addRow(QString::fromUtf8("焦距"), makeValueButtonRow(focalLengthSpin, applyFocalLength, this));
    lensLayout->addLayout(lensForm);
    advancedRoot->addWidget(lensGroup);

    connect(focusPos, &QPushButton::pressed, this, [this, writeCb]() {
        dev_->ir()->ci05FocusStartPositive(this, writeCb);
    });
    connect(focusPos, &QPushButton::released, this, [this, writeCb]() {
        dev_->ir()->ci05FocusStop(this, writeCb);
    });
    connect(focusNeg, &QPushButton::pressed, this, [this, writeCb]() {
        dev_->ir()->ci05FocusStartNegative(this, writeCb);
    });
    connect(focusNeg, &QPushButton::released, this, [this, writeCb]() {
        dev_->ir()->ci05FocusStop(this, writeCb);
    });
    connect(zoomPos, &QPushButton::pressed, this, [this, writeCb]() {
        dev_->ir()->ci05ZoomStartPositive(this, writeCb);
    });
    connect(zoomPos, &QPushButton::released, this, [this, writeCb]() {
        dev_->ir()->ci05ZoomStop(this, writeCb);
    });
    connect(zoomNeg, &QPushButton::pressed, this, [this, writeCb]() {
        dev_->ir()->ci05ZoomStartNegative(this, writeCb);
    });
    connect(zoomNeg, &QPushButton::released, this, [this, writeCb]() {
        dev_->ir()->ci05ZoomStop(this, writeCb);
    });
    connect(focusStepPos, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05FocusStepPositive(this, writeCb);
    });
    connect(focusStepNeg, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05FocusStepNegative(this, writeCb);
    });
    connect(zoomStepPos, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05ZoomStepPositive(this, writeCb);
    });
    connect(zoomStepNeg, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05ZoomStepNegative(this, writeCb);
    });
    connect(autoFocus, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05AutoFocus(this, writeCb);
    });
    connect(applyFov, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetFov(this, static_cast<quint8>(ci05FovCombo_->currentData().toInt()), writeCb);
    });
    connect(applyFocusSpeed, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetFocusSpeed(this, static_cast<quint8>(ci05FocusSpeedSpin_->value()), writeCb);
    });
    connect(applyZoomSpeed, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetZoomSpeed(this, static_cast<quint8>(ci05ZoomSpeedSpin_->value()), writeCb);
    });
    connect(shutterOpen, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05ShutterOpen(this, writeCb);
    });
    connect(shutterClose, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05ShutterClose(this, writeCb);
    });
    connect(callPreset, &QPushButton::clicked, this, [this, presetSpin, writeCb]() {
        dev_->ir()->ci05CallPreset(this, static_cast<quint8>(presetSpin->value()), writeCb);
    });
    connect(setPreset, &QPushButton::clicked, this, [this, presetSpin, writeCb]() {
        dev_->ir()->ci05SetPreset(this, static_cast<quint8>(presetSpin->value()), writeCb);
    });
    connect(applyFocalLength, &QPushButton::clicked, this, [this, focalLengthSpin, writeCb]() {
        dev_->ir()->ci05SetFocalLengthMmX10(this, static_cast<quint16>(focalLengthSpin->value()), writeCb);
    });

    auto* imageGroup = new QGroupBox(QString::fromUtf8("CI05 图像参数"), this);
    auto* imageForm = new QFormLayout(imageGroup);
    ci05BrightnessSpin_ = makeSpin(0, 100, QStringLiteral("irCi05BrightnessSpin"));
    auto* applyBrightness = new QPushButton(QString::fromUtf8("设亮度"), this);
    applyBrightness->setObjectName(QStringLiteral("irCi05ApplyBrightnessButton"));
    imageForm->addRow(QString::fromUtf8("亮度"), makeValueButtonRow(ci05BrightnessSpin_, applyBrightness, this));

    ci05ContrastSpin_ = makeSpin(0, 100, QStringLiteral("irCi05ContrastSpin"));
    auto* applyContrast = new QPushButton(QString::fromUtf8("设对比度"), this);
    imageForm->addRow(QString::fromUtf8("对比度"), makeValueButtonRow(ci05ContrastSpin_, applyContrast, this));

    ci05OverallBrightnessSpin_ = makeSpin(0, 100, QStringLiteral("irCi05OverallBrightnessSpin"));
    auto* applyOverallBrightness = new QPushButton(QString::fromUtf8("设全局亮度"), this);
    applyOverallBrightness->setObjectName(QStringLiteral("irCi05ApplyOverallBrightnessButton"));
    imageForm->addRow(QString::fromUtf8("全局亮度"), makeValueButtonRow(ci05OverallBrightnessSpin_, applyOverallBrightness, this));

    ci05OverallContrastSpin_ = makeSpin(0, 100, QStringLiteral("irCi05OverallContrastSpin"));
    auto* applyOverallContrast = new QPushButton(QString::fromUtf8("设全局对比度"), this);
    imageForm->addRow(QString::fromUtf8("全局对比度"), makeValueButtonRow(ci05OverallContrastSpin_, applyOverallContrast, this));

    ci05SharpnessSpin_ = makeSpin(0, 7, QStringLiteral("irCi05SharpnessSpin"));
    auto* applySharpness = new QPushButton(QString::fromUtf8("设锐化"), this);
    imageForm->addRow(QString::fromUtf8("锐化"), makeValueButtonRow(ci05SharpnessSpin_, applySharpness, this));

    ci05EzoomCombo_ = new QComboBox(this);
    ci05EzoomCombo_->setObjectName(QStringLiteral("irCi05EzoomCombo"));
    ci05EzoomCombo_->addItem(QStringLiteral("1X"), 0);
    ci05EzoomCombo_->addItem(QStringLiteral("2X"), 1);
    ci05EzoomCombo_->addItem(QStringLiteral("4X"), 2);
    auto* applyEzoom = new QPushButton(QString::fromUtf8("设电子放大"), this);
    imageForm->addRow(QString::fromUtf8("电子放大"), makeValueButtonRow(ci05EzoomCombo_, applyEzoom, this));

    ci05FreezeCombo_ = new QComboBox(this);
    ci05FreezeCombo_->setObjectName(QStringLiteral("irCi05FreezeCombo"));
    ci05FreezeCombo_->addItem(QString::fromUtf8("关"), 0);
    ci05FreezeCombo_->addItem(QString::fromUtf8("开"), 1);
    auto* applyFreeze = new QPushButton(QString::fromUtf8("设冻结"), this);
    imageForm->addRow(QString::fromUtf8("图像冻结"), makeValueButtonRow(ci05FreezeCombo_, applyFreeze, this));

    ci05AgcCombo_ = new QComboBox(this);
    ci05AgcCombo_->setObjectName(QStringLiteral("irCi05AgcCombo"));
    ci05AgcCombo_->addItem(QString::fromUtf8("自动"), 0);
    ci05AgcCombo_->addItem(QString::fromUtf8("手动"), 1);
    auto* applyAgc = new QPushButton(QString::fromUtf8("设 AGC"), this);
    imageForm->addRow(QString::fromUtf8("AGC"), makeValueButtonRow(ci05AgcCombo_, applyAgc, this));

    ci05MirrorCombo_ = makeCombo(QStringLiteral("irCi05MirrorCombo"));
    ci05MirrorCombo_->addItem(QString::fromUtf8("正常"), 0);
    ci05MirrorCombo_->addItem(QString::fromUtf8("水平"), 1);
    ci05MirrorCombo_->addItem(QString::fromUtf8("垂直"), 2);
    ci05MirrorCombo_->addItem(QString::fromUtf8("水平+垂直"), 3);
    auto* applyMirror = new QPushButton(QString::fromUtf8("设镜像"), this);
    applyMirror->setObjectName(QStringLiteral("irCi05ApplyMirrorButton"));
    imageForm->addRow(QString::fromUtf8("镜像模式"), makeValueButtonRow(ci05MirrorCombo_, applyMirror, this));

    ci05PaletteCombo_ = makeCombo(QStringLiteral("irCi05PaletteCombo"));
    addRangeItems(ci05PaletteCombo_, QString::fromUtf8("伪彩 "), 0, 7);
    auto* applyPalette = new QPushButton(QString::fromUtf8("设极性/伪彩"), this);
    imageForm->addRow(QString::fromUtf8("极性/伪彩"), makeValueButtonRow(ci05PaletteCombo_, applyPalette, this));

    ci05Y8LevelCombo_ = makeCombo(QStringLiteral("irCi05Y8LevelCombo"));
    addRangeItems(ci05Y8LevelCombo_, QString::fromUtf8("档位 "), 0, 5);
    auto* applyY8Level = new QPushButton(QString::fromUtf8("设 Y8 档位"), this);
    imageForm->addRow(QString::fromUtf8("Y8 档位"), makeValueButtonRow(ci05Y8LevelCombo_, applyY8Level, this));

    auto* saveParams = new QPushButton(QString::fromUtf8("保存参数"), this);
    saveParams->setObjectName(QStringLiteral("irCi05SaveParamsButton"));
    imageForm->addRow(saveParams);
    advancedRoot->addWidget(imageGroup);

    connect(applyBrightness, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetBrightness(this, static_cast<quint8>(ci05BrightnessSpin_->value()), writeCb);
    });
    connect(applyContrast, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetContrast(this, static_cast<quint8>(ci05ContrastSpin_->value()), writeCb);
    });
    connect(applyOverallBrightness, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetOverallBrightness(this, static_cast<quint8>(ci05OverallBrightnessSpin_->value()), writeCb);
    });
    connect(applyOverallContrast, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetOverallContrast(this, static_cast<quint8>(ci05OverallContrastSpin_->value()), writeCb);
    });
    connect(applySharpness, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetSharpness(this, static_cast<quint8>(ci05SharpnessSpin_->value()), writeCb);
    });
    connect(applyEzoom, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetEzoom(this, static_cast<quint8>(ci05EzoomCombo_->currentData().toInt()), writeCb);
    });
    connect(applyFreeze, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetFreeze(this, static_cast<quint8>(ci05FreezeCombo_->currentData().toInt()), writeCb);
    });
    connect(applyAgc, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetAgcMode(this, static_cast<quint8>(ci05AgcCombo_->currentData().toInt()), writeCb);
    });
    connect(applyMirror, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetMirrorMode(this, comboValue(ci05MirrorCombo_), writeCb);
    });
    connect(applyPalette, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetPolarityPalette(this, comboValue(ci05PaletteCombo_), writeCb);
    });
    connect(applyY8Level, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetY8Level(this, comboValue(ci05Y8LevelCombo_), writeCb);
    });
    connect(saveParams, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SaveParams(this, writeCb);
    });

    auto* integrationGroup = new QGroupBox(QString::fromUtf8("CI05 积分 / 帧频 / 本底"), this);
    auto* integrationForm = new QFormLayout(integrationGroup);
    ci05FrameRateSpin_ = makeSpin(0, 65535, QStringLiteral("irCi05FrameRateSpin"));
    auto* applyFrameRate = new QPushButton(QString::fromUtf8("设帧频 0.01 Hz"), this);
    applyFrameRate->setObjectName(QStringLiteral("irCi05ApplyFrameRateButton"));
    integrationForm->addRow(QString::fromUtf8("帧频编码值"), makeValueButtonRow(ci05FrameRateSpin_, applyFrameRate, this));

    auto* readFrameRate = new QPushButton(QString::fromUtf8("读帧频"), this);
    auto* frameRateReadout = makeReadoutLabel(QString::fromUtf8("帧频"), this);
    integrationForm->addRow(QString::fromUtf8("读取"), makeReadoutButtonRow(frameRateReadout, readFrameRate, this));

    ci05IntegrationMcSpin_ = makeSpin(0, 16777215, QStringLiteral("irCi05IntegrationMcSpin"));
    auto* applyIntegrationMc = new QPushButton(QString::fromUtf8("设 CLK 积分"), this);
    integrationForm->addRow(QString::fromUtf8("CLK 积分"), makeValueButtonRow(ci05IntegrationMcSpin_, applyIntegrationMc, this));

    ci05IntegrationGearCombo_ = makeCombo(QStringLiteral("irCi05IntegrationGearCombo"));
    addRangeItems(ci05IntegrationGearCombo_, QString::fromUtf8("档位 "), 0, 2);
    auto* applyIntegrationGear = new QPushButton(QString::fromUtf8("设积分档位"), this);
    applyIntegrationGear->setObjectName(QStringLiteral("irCi05ApplyIntegrationGearButton"));
    integrationForm->addRow(QString::fromUtf8("积分档位"), makeValueButtonRow(ci05IntegrationGearCombo_, applyIntegrationGear, this));

    ci05IntegrationAutoCombo_ = makeCombo(QStringLiteral("irCi05IntegrationAutoCombo"));
    ci05IntegrationAutoCombo_->addItem(QString::fromUtf8("手动"), 0);
    ci05IntegrationAutoCombo_->addItem(QString::fromUtf8("自动"), 1);
    auto* applyIntegrationAuto = new QPushButton(QString::fromUtf8("设积分档自动"), this);
    integrationForm->addRow(QString::fromUtf8("积分档自动"), makeValueButtonRow(ci05IntegrationAutoCombo_, applyIntegrationAuto, this));

    ci05BackgroundGearCombo_ = makeCombo(QStringLiteral("irCi05BackgroundGearCombo"));
    addRangeItems(ci05BackgroundGearCombo_, QString::fromUtf8("档位 "), 0, 2);
    auto* applyBackgroundGear = new QPushButton(QString::fromUtf8("设本底档位"), this);
    integrationForm->addRow(QString::fromUtf8("本底档位"), makeValueButtonRow(ci05BackgroundGearCombo_, applyBackgroundGear, this));

    ci05BackgroundAutoCombo_ = makeCombo(QStringLiteral("irCi05BackgroundAutoCombo"));
    ci05BackgroundAutoCombo_->addItem(QString::fromUtf8("手动"), 0);
    ci05BackgroundAutoCombo_->addItem(QString::fromUtf8("自动"), 1);
    auto* applyBackgroundAuto = new QPushButton(QString::fromUtf8("设本底档自动"), this);
    integrationForm->addRow(QString::fromUtf8("本底档自动"), makeValueButtonRow(ci05BackgroundAutoCombo_, applyBackgroundAuto, this));
    advancedRoot->addWidget(integrationGroup);

    connect(applyIntegration, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetIntegrationMsX10(this, static_cast<quint16>(ci05IntegrationMsSpin_->value()), writeCb);
    });
    connect(integrationIncrease, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05IntegrationIncrease0p1Ms(this, writeCb);
    });
    connect(integrationDecrease, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05IntegrationDecrease0p1Ms(this, writeCb);
    });
    connect(applyFrameRate, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetFrameRateHzX100(this, static_cast<quint16>(ci05FrameRateSpin_->value()), writeCb);
    });
    connect(applyIntegrationMc, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetIntegrationMc(this, static_cast<quint32>(ci05IntegrationMcSpin_->value()), writeCb);
    });
    connect(applyIntegrationGear, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetIntegrationGear(this, comboValue(ci05IntegrationGearCombo_), writeCb);
    });
    connect(applyIntegrationAuto, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetIntegrationGearAuto(this, comboValue(ci05IntegrationAutoCombo_), writeCb);
    });
    connect(applyBackgroundGear, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetBackgroundGear(this, comboValue(ci05BackgroundGearCombo_), writeCb);
    });
    connect(applyBackgroundAuto, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetBackgroundGearAuto(this, comboValue(ci05BackgroundAutoCombo_), writeCb);
    });
    connect(readFrameRate, &QPushButton::clicked, this, [this, frameRateReadout]() {
        dev_->ir()->ci05ReadFrameRateHz(this, [this, frameRateReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(frameRateReadout, QString::fromUtf8("帧频"), ok, data, err, QStringLiteral("Hz"));
        });
    });

    auto* compensationGroup = new QGroupBox(QString::fromUtf8("CI05 高级补偿"), this);
    auto* compensationLayout = new QVBoxLayout(compensationGroup);
    auto* defocusCompensation = new QPushButton(QString::fromUtf8("离焦补偿"), this);
    defocusCompensation->setObjectName(QStringLiteral("irCi05TriggerDefocusCompensationButton"));
    auto* integrationCorrection = new QPushButton(QString::fromUtf8("积分时间校正"), this);
    integrationCorrection->setObjectName(QStringLiteral("irCi05TriggerIntegrationCorrectionButton"));
    compensationLayout->addLayout(makeGrid2Col({defocusCompensation, integrationCorrection}));
    auto* compensationForm = new QFormLayout();
    ci05BootCompensationModeCombo_ = makeCombo(QStringLiteral("irCi05BootCompensationModeCombo"));
    addRangeItems(ci05BootCompensationModeCombo_, QString::fromUtf8("模式 "), 0, 3);
    auto* applyBootCompensationMode = new QPushButton(QString::fromUtf8("设开机补偿"), this);
    compensationForm->addRow(QString::fromUtf8("开机补偿模式"), makeValueButtonRow(ci05BootCompensationModeCombo_, applyBootCompensationMode, this));
    ci05GearSwitchCompensationModeCombo_ = makeCombo(QStringLiteral("irCi05GearSwitchCompensationModeCombo"));
    addRangeItems(ci05GearSwitchCompensationModeCombo_, QString::fromUtf8("模式 "), 0, 4);
    auto* applyGearSwitchCompensationMode = new QPushButton(QString::fromUtf8("设换档补偿"), this);
    compensationForm->addRow(QString::fromUtf8("换档补偿模式"), makeValueButtonRow(ci05GearSwitchCompensationModeCombo_, applyGearSwitchCompensationMode, this));
    compensationLayout->addLayout(compensationForm);
    advancedRoot->addWidget(compensationGroup);

    connect(shutterCompensation, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05TriggerShutterCompensation(this, writeCb);
    });
    connect(sceneCompensation, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05TriggerSceneCompensation(this, writeCb);
    });
    connect(defocusCompensation, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05TriggerDefocusCompensation(this, writeCb);
    });
    connect(integrationCorrection, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05TriggerIntegrationCorrection(this, writeCb);
    });
    connect(applyBootCompensationMode, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetBootCompensationMode(this, comboValue(ci05BootCompensationModeCombo_), writeCb);
    });
    connect(applyGearSwitchCompensationMode, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetGearSwitchCompensationMode(this, comboValue(ci05GearSwitchCompensationModeCombo_), writeCb);
    });

    auto* outputGroup = new QGroupBox(QString::fromUtf8("CI05 同步 / 输出"), this);
    auto* outputForm = new QFormLayout(outputGroup);
    ci05SyncModeCombo_ = makeCombo(QStringLiteral("irCi05SyncModeCombo"));
    addRangeItems(ci05SyncModeCombo_, QString::fromUtf8("模式 "), 0, 2);
    auto* applySyncMode = new QPushButton(QString::fromUtf8("设同步模式"), this);
    applySyncMode->setObjectName(QStringLiteral("irCi05ApplySyncModeButton"));
    outputForm->addRow(QString::fromUtf8("同步模式"), makeValueButtonRow(ci05SyncModeCombo_, applySyncMode, this));
    ci05VideoSourceCombo_ = makeCombo(QStringLiteral("irCi05VideoSourceCombo"));
    addOffOnItems(ci05VideoSourceCombo_);
    auto* applyVideoSource = new QPushButton(QString::fromUtf8("设视频源"), this);
    outputForm->addRow(QString::fromUtf8("视频源"), makeValueButtonRow(ci05VideoSourceCombo_, applyVideoSource, this));
    ci05ParamLineCombo_ = makeCombo(QStringLiteral("irCi05ParamLineCombo"));
    addOffOnItems(ci05ParamLineCombo_);
    auto* applyParamLine = new QPushButton(QString::fromUtf8("设参数行"), this);
    outputForm->addRow(QString::fromUtf8("参数行"), makeValueButtonRow(ci05ParamLineCombo_, applyParamLine, this));
    ci05DigitalFormatCombo_ = makeCombo(QStringLiteral("irCi05DigitalFormatCombo"));
    addRangeItems(ci05DigitalFormatCombo_, QString::fromUtf8("格式 "), 0, 2);
    auto* applyDigitalFormat = new QPushButton(QString::fromUtf8("设数字格式"), this);
    outputForm->addRow(QString::fromUtf8("数字格式"), makeValueButtonRow(ci05DigitalFormatCombo_, applyDigitalFormat, this));
    ci05TestPatternCombo_ = makeCombo(QStringLiteral("irCi05TestPatternCombo"));
    addRangeItems(ci05TestPatternCombo_, QString::fromUtf8("测试图 "), 0, 6);
    auto* applyTestPattern = new QPushButton(QString::fromUtf8("设测试图"), this);
    outputForm->addRow(QString::fromUtf8("测试图"), makeValueButtonRow(ci05TestPatternCombo_, applyTestPattern, this));
    ci05ImageModeCombo_ = makeCombo(QStringLiteral("irCi05ImageModeCombo"));
    addRangeItems(ci05ImageModeCombo_, QString::fromUtf8("模式 "), 0, 3);
    auto* applyImageMode = new QPushButton(QString::fromUtf8("设图像模式"), this);
    outputForm->addRow(QString::fromUtf8("图像模式"), makeValueButtonRow(ci05ImageModeCombo_, applyImageMode, this));
    ci05StatusOutputModeCombo_ = makeCombo(QStringLiteral("irCi05StatusOutputModeCombo"));
    addRangeItems(ci05StatusOutputModeCombo_, QString::fromUtf8("模式 "), 0, 2);
    auto* applyStatusOutputMode = new QPushButton(QString::fromUtf8("设状态输出"), this);
    outputForm->addRow(QString::fromUtf8("状态输出"), makeValueButtonRow(ci05StatusOutputModeCombo_, applyStatusOutputMode, this));
    ci05TmodFilterSpin_ = makeSpin(0, 94, QStringLiteral("irCi05TmodFilterSpin"));
    auto* applyTmodFilter = new QPushButton(QString::fromUtf8("设 TMOD"), this);
    outputForm->addRow(QString::fromUtf8("TMOD 滤波"), makeValueButtonRow(ci05TmodFilterSpin_, applyTmodFilter, this));
    ci05NtmFilterSpin_ = makeSpin(0, 10, QStringLiteral("irCi05NtmFilterSpin"));
    auto* applyNtmFilter = new QPushButton(QString::fromUtf8("设 nTM"), this);
    outputForm->addRow(QString::fromUtf8("nTM 滤波"), makeValueButtonRow(ci05NtmFilterSpin_, applyNtmFilter, this));
    ci05VerticalStripeRemovalCombo_ = makeCombo(QStringLiteral("irCi05VerticalStripeRemovalCombo"));
    addOffOnItems(ci05VerticalStripeRemovalCombo_);
    auto* applyVerticalStripeRemoval = new QPushButton(QString::fromUtf8("设去竖条"), this);
    outputForm->addRow(QString::fromUtf8("去竖条"), makeValueButtonRow(ci05VerticalStripeRemovalCombo_, applyVerticalStripeRemoval, this));
    advancedRoot->addWidget(outputGroup);

    connect(applySyncMode, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetSyncMode(this, comboValue(ci05SyncModeCombo_), writeCb);
    });
    connect(applyVideoSource, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetVideoSource(this, comboValue(ci05VideoSourceCombo_), writeCb);
    });
    connect(applyParamLine, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetParamLine(this, comboValue(ci05ParamLineCombo_), writeCb);
    });
    connect(applyDigitalFormat, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetDigitalFormat(this, comboValue(ci05DigitalFormatCombo_), writeCb);
    });
    connect(applyTestPattern, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetTestPattern(this, comboValue(ci05TestPatternCombo_), writeCb);
    });
    connect(applyImageMode, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetImageMode(this, comboValue(ci05ImageModeCombo_), writeCb);
    });
    connect(applyStatusOutputMode, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetStatusOutputMode(this, comboValue(ci05StatusOutputModeCombo_), writeCb);
    });
    connect(applyTmodFilter, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetTmodFilter(this, static_cast<quint8>(ci05TmodFilterSpin_->value()), writeCb);
    });
    connect(applyNtmFilter, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->ci05SetNtmFilter(this, static_cast<quint8>(ci05NtmFilterSpin_->value()), writeCb);
    });
    connect(applyVerticalStripeRemoval, &QPushButton::clicked, this, [this, comboValue, writeCb]() {
        dev_->ir()->ci05SetVerticalStripeRemoval(this, comboValue(ci05VerticalStripeRemovalCombo_), writeCb);
    });

    auto* menuGroup = new QGroupBox(QString::fromUtf8("CI05 菜单 / OSD"), this);
    auto* menuLayout = new QVBoxLayout(menuGroup);
    auto* menuUser = new QPushButton(QString::fromUtf8("用户键"), this);
    auto* menuLeft = new QPushButton(QString::fromUtf8("左"), this);
    auto* menuRight = new QPushButton(QString::fromUtf8("右"), this);
    auto* menuParamInc = new QPushButton(QString::fromUtf8("参数 +"), this);
    auto* menuParamDec = new QPushButton(QString::fromUtf8("参数 -"), this);
    auto* promptOn = new QPushButton(QString::fromUtf8("提示打开"), this);
    auto* promptOff = new QPushButton(QString::fromUtf8("提示关闭"), this);
    menuLayout->addLayout(makeGrid2Col({menuUser, menuLeft, menuRight, menuParamInc,
                                        menuParamDec, promptOn, promptOff}));
    advancedRoot->addWidget(menuGroup);

    connect(menuUser, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05MenuUser(this, writeCb); });
    connect(menuLeft, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05MenuLeft(this, writeCb); });
    connect(menuRight, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05MenuRight(this, writeCb); });
    connect(menuParamInc, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05MenuParamInc(this, writeCb); });
    connect(menuParamDec, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05MenuParamDec(this, writeCb); });
    connect(promptOn, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05PromptOn(this, writeCb); });
    connect(promptOff, &QPushButton::clicked, this, [this, writeCb]() { dev_->ir()->ci05PromptOff(this, writeCb); });

    auto* statusGroup = new QGroupBox(QString::fromUtf8("CI05 状态读取"), this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    ci05ActionStatusLabel_ = new QLabel("-", this);
    ci05ActionStatusLabel_->setWordWrap(true);
    ci05ActionStatusLabel_->setProperty("secondary", true);
    auto* workStateReadout = makeReadoutLabel(QString::fromUtf8("工作状态"), this);
    auto* readWorkState = new QPushButton(QString::fromUtf8("读工作状态"), this);
    statusLayout->addWidget(makeReadoutButtonRow(workStateReadout, readWorkState, this));
    auto* status1Readout = makeReadoutLabel(QStringLiteral("Status1"), this);
    auto* readStatus1 = new QPushButton(QStringLiteral("读 Status1"), this);
    readStatus1->setObjectName(QStringLiteral("irCi05ReadStatus1Button"));
    statusLayout->addWidget(makeReadoutButtonRow(status1Readout, readStatus1, this));
    auto* status2Readout = makeReadoutLabel(QStringLiteral("Status2"), this);
    auto* readStatus2 = new QPushButton(QStringLiteral("读 Status2"), this);
    readStatus2->setObjectName(QStringLiteral("irCi05ReadStatus2Button"));
    statusLayout->addWidget(makeReadoutButtonRow(status2Readout, readStatus2, this));
    auto* status3Readout = makeReadoutLabel(QStringLiteral("Status3"), this);
    auto* readStatus3 = new QPushButton(QStringLiteral("读 Status3"), this);
    readStatus3->setObjectName(QStringLiteral("irCi05ReadStatus3Button"));
    statusLayout->addWidget(makeReadoutButtonRow(status3Readout, readStatus3, this));
    auto* status4Readout = makeReadoutLabel(QStringLiteral("Status4"), this);
    auto* readStatus4 = new QPushButton(QStringLiteral("读 Status4"), this);
    readStatus4->setObjectName(QStringLiteral("irCi05ReadStatus4Button"));
    statusLayout->addWidget(makeReadoutButtonRow(status4Readout, readStatus4, this));
    auto* serialReadout = makeReadoutLabel(QString::fromUtf8("序列号"), this);
    auto* readSerial = new QPushButton(QString::fromUtf8("读序列号"), this);
    statusLayout->addWidget(makeReadoutButtonRow(serialReadout, readSerial, this));
    auto* workMinutesReadout = makeReadoutLabel(QString::fromUtf8("工作时长"), this);
    auto* readWorkMinutes = new QPushButton(QString::fromUtf8("读工作时长"), this);
    statusLayout->addWidget(makeReadoutButtonRow(workMinutesReadout, readWorkMinutes, this));
    auto* coolingDoneReadout = makeReadoutLabel(QString::fromUtf8("制冷完成"), this);
    auto* readCoolingDone = new QPushButton(QString::fromUtf8("读制冷完成"), this);
    statusLayout->addWidget(makeReadoutButtonRow(coolingDoneReadout, readCoolingDone, this));
    auto* ci05SelfCheckReadout = makeReadoutLabel(QString::fromUtf8("自检"), this);
    auto* readCi05SelfCheck = new QPushButton(QString::fromUtf8("读自检"), this);
    statusLayout->addWidget(makeReadoutButtonRow(ci05SelfCheckReadout, readCi05SelfCheck, this));
    auto* focalLengthReadout = makeReadoutLabel(QString::fromUtf8("焦距"), this);
    auto* readFocalLength = new QPushButton(QString::fromUtf8("读焦距"), this);
    statusLayout->addWidget(makeReadoutButtonRow(focalLengthReadout, readFocalLength, this));
    auto* zoomMotorReadout = makeReadoutLabel(QString::fromUtf8("变倍电机"), this);
    auto* readZoomMotor = new QPushButton(QString::fromUtf8("读变倍电机"), this);
    statusLayout->addWidget(makeReadoutButtonRow(zoomMotorReadout, readZoomMotor, this));
    auto* focusMotorReadout = makeReadoutLabel(QString::fromUtf8("调焦电机"), this);
    auto* readFocusMotor = new QPushButton(QString::fromUtf8("读调焦电机"), this);
    statusLayout->addWidget(makeReadoutButtonRow(focusMotorReadout, readFocusMotor, this));
    statusLayout->addWidget(ci05ActionStatusLabel_);
    advancedRoot->addWidget(statusGroup);

    connect(readWorkState, &QPushButton::clicked, this, [this, workStateReadout]() {
        dev_->ir()->ci05ReadWorkState(this, [this, workStateReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(workStateReadout, QString::fromUtf8("工作状态"), ok, data, err);
        });
    });
    connect(readStatus1, &QPushButton::clicked, this, [this, status1Readout]() {
        dev_->ir()->ci05ReadStatus1(this, [this, status1Readout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(status1Readout, QStringLiteral("Status1"), ok, data, err);
        });
    });
    connect(readStatus2, &QPushButton::clicked, this, [this, status2Readout]() {
        dev_->ir()->ci05ReadStatus2(this, [this, status2Readout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(status2Readout, QStringLiteral("Status2"), ok, data, err);
        });
    });
    connect(readStatus3, &QPushButton::clicked, this, [this, status3Readout]() {
        dev_->ir()->ci05ReadStatus3(this, [this, status3Readout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(status3Readout, QStringLiteral("Status3"), ok, data, err);
        });
    });
    connect(readStatus4, &QPushButton::clicked, this, [this, status4Readout]() {
        dev_->ir()->ci05ReadStatus4(this, [this, status4Readout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(status4Readout, QStringLiteral("Status4"), ok, data, err);
        });
    });
    connect(readSerial, &QPushButton::clicked, this, [this, serialReadout]() {
        dev_->ir()->ci05ReadSerialNumber(this, [this, serialReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(serialReadout, QString::fromUtf8("序列号"), ok, data, err);
        });
    });
    connect(readWorkMinutes, &QPushButton::clicked, this, [this, workMinutesReadout]() {
        dev_->ir()->ci05ReadWorkMinutes(this, [this, workMinutesReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(workMinutesReadout, QString::fromUtf8("工作时长"), ok, data, err);
        });
    });
    connect(readCoolingDone, &QPushButton::clicked, this, [this, coolingDoneReadout]() {
        dev_->ir()->ci05ReadCoolingDoneSeconds(this, [this, coolingDoneReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(coolingDoneReadout, QString::fromUtf8("制冷完成"), ok, data, err, QStringLiteral("s"));
        });
    });
    connect(readCi05SelfCheck, &QPushButton::clicked, this, [this, ci05SelfCheckReadout]() {
        dev_->ir()->ci05ReadSelfCheck(this, [this, ci05SelfCheckReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(ci05SelfCheckReadout, QString::fromUtf8("自检"), ok, data, err);
        });
    });
    connect(readFocalLength, &QPushButton::clicked, this, [this, focalLengthReadout]() {
        dev_->ir()->ci05QueryFocalLength(this, [this, focalLengthReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(focalLengthReadout, QString::fromUtf8("焦距"), ok, data, err);
        });
    });
    connect(readZoomMotor, &QPushButton::clicked, this, [this, zoomMotorReadout]() {
        dev_->ir()->ci05QueryZoomMotorPosition(this, [this, zoomMotorReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(zoomMotorReadout, QString::fromUtf8("变倍电机"), ok, data, err);
        });
    });
    connect(readFocusMotor, &QPushButton::clicked, this, [this, focusMotorReadout]() {
        dev_->ir()->ci05QueryFocusMotorPosition(this, [this, focusMotorReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(focusMotorReadout, QString::fromUtf8("调焦电机"), ok, data, err);
        });
    });

    root->addWidget(advancedGroup);
    root->addStretch();
}

// ---- 亮度 / 对比度 / DDE / AB 模式 ----

void IrPanel::setupImageParams(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("亮度 / 对比度 / DDE"), this);
    auto* form = new QFormLayout(grp);

    auto makeSpin = [this](int mx) {
        auto* spin = new QSpinBox(this);
        spin->setRange(0, mx);
        return spin;
    };

    brightSpin_ = makeSpin(255);
    brightSpin_->setObjectName(QStringLiteral("irBrightnessSpin"));
    contrastSpin_ = makeSpin(255);
    contrastSpin_->setObjectName(QStringLiteral("irContrastSpin"));
    ddeSpin_ = makeSpin(15);
    ddeSpin_->setObjectName(QStringLiteral("irDdeSpin"));

    auto* applyBright   = new QPushButton(QString::fromUtf8("设亮度"), this);
    applyBright->setObjectName(QStringLiteral("irApplyBrightnessButton"));
    auto* applyContrast = new QPushButton(QString::fromUtf8("设对比度"), this);
    applyContrast->setObjectName(QStringLiteral("irApplyContrastButton"));
    auto* applyDde      = new QPushButton(QString::fromUtf8("设 DDE"), this);
    form->addRow(QString::fromUtf8("亮度"), makeValueButtonRow(brightSpin_, applyBright, this));
    form->addRow(QString::fromUtf8("对比度"), makeValueButtonRow(contrastSpin_, applyContrast, this));
    form->addRow(QString::fromUtf8("DDE"), makeValueButtonRow(ddeSpin_, applyDde, this));

    abModeCombo_ = new QComboBox(this);
    abModeCombo_->setObjectName(QStringLiteral("irAbModeCombo"));
    abModeCombo_->addItem(QString::fromUtf8("手动"), 0);
    abModeCombo_->addItem(QString::fromUtf8("自动"), 1);
    auto* applyAbMode   = new QPushButton(QString::fromUtf8("设 AB 模式"), this);
    form->addRow(QString::fromUtf8("AB 模式"), makeValueButtonRow(abModeCombo_, applyAbMode, this));
    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };

    connect(applyBright, &QPushButton::clicked, this, [this, writeCb]() {
        const quint8 value = static_cast<quint8>(brightSpin_->value());
        dev_->ir()->setBrightness(this, value, writeCb);
    });
    connect(applyContrast, &QPushButton::clicked, this, [this, writeCb]() {
        const quint8 value = static_cast<quint8>(contrastSpin_->value());
        dev_->ir()->setContrast(this, value, writeCb);
    });
    connect(applyDde, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setDde(this, static_cast<quint8>(ddeSpin_->value()), writeCb);
    });
    connect(applyAbMode, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setAbMode(this, static_cast<quint8>(abModeCombo_->currentData().toInt()), writeCb);
    });
}

// ---- 积分时间 ----

void IrPanel::setupIntegration(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("积分时间"), this);
    auto* form = new QFormLayout(grp);

    integSpin_ = new QSpinBox(this);
    integSpin_->setObjectName(QStringLiteral("irIntegrationSpin"));
    integSpin_->setRange(0, 65535);
    auto* applyInteg = new QPushButton(QString::fromUtf8("设积分"), this);
    form->addRow(QString::fromUtf8("积分时间"), makeValueButtonRow(integSpin_, applyInteg, this));

    integModeCombo_ = new QComboBox(this);
    integModeCombo_->setObjectName(QStringLiteral("irIntegrationModeCombo"));
    integModeCombo_->addItem(QString::fromUtf8("自动积分"), 0);
    integModeCombo_->addItem(QString::fromUtf8("手动积分"), 1);
    auto* applyIntegMode = new QPushButton(QString::fromUtf8("设积分模式"), this);
    form->addRow(QString::fromUtf8("积分模式"), makeValueButtonRow(integModeCombo_, applyIntegMode, this));

    gearModeCombo_ = new QComboBox(this);
    gearModeCombo_->setObjectName(QStringLiteral("irGearModeCombo"));
    gearModeCombo_->addItem(QString::fromUtf8("手动切换"), 0);
    gearModeCombo_->addItem(QString::fromUtf8("自动切换"), 1);
    auto* applyGearMode = new QPushButton(QString::fromUtf8("设档位模式"), this);
    form->addRow(QString::fromUtf8("档位模式"), makeValueButtonRow(gearModeCombo_, applyGearMode, this));

    gearSelectCombo_ = new QComboBox(this);
    gearSelectCombo_->setObjectName(QStringLiteral("irGearSelectCombo"));
    for (int i = 0; i < 8; ++i)
        gearSelectCombo_->addItem(QString::number(i), i);
    auto* applyGear = new QPushButton(QString::fromUtf8("选积分档"), this);
    form->addRow(QString::fromUtf8("档位选择"), makeValueButtonRow(gearSelectCombo_, applyGear, this));

    auto* queryIntBtn = new QPushButton(QString::fromUtf8("查询积分时间"), this);
    auto* integReadout = makeReadoutLabel(QString::fromUtf8("积分时间"), this);
    form->addRow(QString::fromUtf8("读取"), makeReadoutButtonRow(integReadout, queryIntBtn, this));

    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };

    connect(applyInteg, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setIntegration(this, static_cast<quint16>(integSpin_->value()), writeCb);
    });
    connect(applyIntegMode, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setManualIntegration(this, static_cast<quint8>(integModeCombo_->currentData().toInt()), writeCb);
    });
    connect(applyGearMode, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setIntegrationGearMode(this, static_cast<quint8>(gearModeCombo_->currentData().toInt()), writeCb);
    });
    connect(applyGear, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->selectIntegrationGear(this, static_cast<quint8>(gearSelectCombo_->currentData().toInt()), writeCb);
    });
    connect(queryIntBtn, &QPushButton::clicked, this, [this, integReadout]() {
        dev_->ir()->queryIntegrationTime(this, [this, integReadout](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(integReadout, QString::fromUtf8("积分时间"), ok, data, err);
        });
    });
}

// ---- 图像显示 ----

void IrPanel::setupImageDisplay(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("图像显示"), this);
    auto* form = new QFormLayout(grp);

    imageTypeCombo_ = new QComboBox(this);
    imageTypeCombo_->setObjectName(QStringLiteral("irImageTypeCombo"));
    imageTypeCombo_->addItem(QString::fromUtf8("增强 8bit"), 0);
    imageTypeCombo_->addItem(QString::fromUtf8("14bit 原始"), 1);
    imageTypeCombo_->addItem(QString::fromUtf8("14bit 预处理"), 2);
    imageTypeCombo_->addItem(QString::fromUtf8("测试图"), 3);
    auto* applyImageType = new QPushButton(QString::fromUtf8("设图像类型"), this);
    form->addRow(QString::fromUtf8("图像类型"), makeValueButtonRow(imageTypeCombo_, applyImageType, this));

    testPatternCombo_ = new QComboBox(this);
    testPatternCombo_->setObjectName(QStringLiteral("irTestPatternCombo"));
    testPatternCombo_->addItem(QString::fromUtf8("竖灰阶"), 0);
    testPatternCombo_->addItem(QString::fromUtf8("横灰阶"), 1);
    testPatternCombo_->addItem(QString::fromUtf8("棋盘格"), 2);
    auto* applyTestPattern = new QPushButton(QString::fromUtf8("设测试图"), this);
    form->addRow(QString::fromUtf8("测试图"), makeValueButtonRow(testPatternCombo_, applyTestPattern, this));

    colorModeCombo_ = new QComboBox(this);
    colorModeCombo_->setObjectName(QStringLiteral("irColorModeCombo"));
    colorModeCombo_->addItem(QString::fromUtf8("白热"), 0);
    colorModeCombo_->addItem(QString::fromUtf8("黑热"), 1);
    auto* applyColorMode = new QPushButton(QString::fromUtf8("设彩色模式"), this);
    form->addRow(QString::fromUtf8("彩色模式"), makeValueButtonRow(colorModeCombo_, applyColorMode, this));

    badPixelDispCombo_ = new QComboBox(this);
    badPixelDispCombo_->setObjectName(QStringLiteral("irBadPixelDisplayCombo"));
    badPixelDispCombo_->addItem(QString::fromUtf8("正常"), 0);
    badPixelDispCombo_->addItem(QString::fromUtf8("高亮"), 1);
    auto* applyBadPixelDisp = new QPushButton(QString::fromUtf8("设坏元显示"), this);
    form->addRow(QString::fromUtf8("坏元显示"), makeValueButtonRow(badPixelDispCombo_, applyBadPixelDisp, this));
    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };

    connect(applyImageType, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setImageType(this, static_cast<quint8>(imageTypeCombo_->currentData().toInt()), writeCb);
    });
    connect(applyTestPattern, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setTestPattern(this, static_cast<quint8>(testPatternCombo_->currentData().toInt()), writeCb);
    });
    connect(applyColorMode, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setColorMode(this, static_cast<quint8>(colorModeCombo_->currentData().toInt()), writeCb);
    });
    connect(applyBadPixelDisp, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setBadPixelDisplayMode(this, static_cast<quint8>(badPixelDispCombo_->currentData().toInt()), writeCb);
    });
}

// ---- 滤波 ----

void IrPanel::setupFilters(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("滤波"), this);
    auto* form = new QFormLayout(grp);

    auto* applyTempFilter = new QPushButton(QString::fromUtf8("设时域滤波"), this);
    configureInlineButton(applyTempFilter);
    {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        tempFilterChk_ = new QCheckBox(QString::fromUtf8("启用"), this);
        tempFilterChk_->setObjectName(QStringLiteral("irTempFilterCheck"));
        tempFilterCoeffSpin_ = new QSpinBox(this);
        tempFilterCoeffSpin_->setObjectName(QStringLiteral("irTempFilterCoeffSpin"));
        tempFilterCoeffSpin_->setRange(1, 15);
        tempFilterCoeffSpin_->setValue(1);
        configureCompactSpin(tempFilterCoeffSpin_);
        row->addWidget(tempFilterChk_);
        row->addWidget(new QLabel(QString::fromUtf8("系数 1-15"), this));
        row->addWidget(tempFilterCoeffSpin_);
        row->addWidget(applyTempFilter);
        form->addRow(QString::fromUtf8("时域滤波"), row);
    }

    auto* applyMedianFilter = new QPushButton(QString::fromUtf8("设中值滤波"), this);
    configureInlineButton(applyMedianFilter);
    {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        medianFilterChk_ = new QCheckBox(QString::fromUtf8("启用"), this);
        medianFilterChk_->setObjectName(QStringLiteral("irMedianFilterCheck"));
        medianFilterCoeffSpin_ = new QSpinBox(this);
        medianFilterCoeffSpin_->setObjectName(QStringLiteral("irMedianFilterCoeffSpin"));
        medianFilterCoeffSpin_->setRange(10, 127);
        medianFilterCoeffSpin_->setValue(10);
        configureCompactSpin(medianFilterCoeffSpin_);
        row->addWidget(medianFilterChk_);
        row->addWidget(new QLabel(QString::fromUtf8("系数 10-127"), this));
        row->addWidget(medianFilterCoeffSpin_);
        row->addWidget(applyMedianFilter);
        form->addRow(QString::fromUtf8("中值滤波"), row);
    }

    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };

    connect(applyTempFilter, &QPushButton::clicked, this, [this, writeCb]() {
        bool en = tempFilterChk_->isChecked();
        quint8 coeff = static_cast<quint8>(tempFilterCoeffSpin_->value());
        dev_->ir()->setTemporalFilter(this, en, coeff, writeCb);
    });
    connect(applyMedianFilter, &QPushButton::clicked, this, [this, writeCb]() {
        bool en = medianFilterChk_->isChecked();
        quint8 coeff = static_cast<quint8>(medianFilterCoeffSpin_->value());
        dev_->ir()->setMedianFilter(this, en, coeff, writeCb);
    });
}

// ---- 翻转与同步 ----

void IrPanel::setupFlipSync(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("翻转与同步"), this);
    auto* form = new QFormLayout(grp);

    auto makeOnOffCombo = [this]() {
        auto* cb = new QComboBox(this);
        cb->addItem(QString::fromUtf8("关"), 0);
        cb->addItem(QString::fromUtf8("开"), 1);
        return cb;
    };

    flipHCombo_ = makeOnOffCombo();
    flipHCombo_->setObjectName(QStringLiteral("irFlipHCombo"));
    auto* applyFlipH = new QPushButton(QString::fromUtf8("设左右翻转"), this);
    form->addRow(QString::fromUtf8("左右翻转"), makeValueButtonRow(flipHCombo_, applyFlipH, this));

    flipVCombo_ = makeOnOffCombo();
    flipVCombo_->setObjectName(QStringLiteral("irFlipVCombo"));
    auto* applyFlipV = new QPushButton(QString::fromUtf8("设上下翻转"), this);
    form->addRow(QString::fromUtf8("上下翻转"), makeValueButtonRow(flipVCombo_, applyFlipV, this));

    extSyncCombo_ = makeOnOffCombo();
    extSyncCombo_->setObjectName(QStringLiteral("irExtSyncCombo"));
    auto* applyExtSync = new QPushButton(QString::fromUtf8("设外同步"), this);
    form->addRow(QString::fromUtf8("外同步"), makeValueButtonRow(extSyncCombo_, applyExtSync, this));
    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };

    connect(applyFlipH, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setFlipHorizontal(this, static_cast<quint8>(flipHCombo_->currentData().toInt()), writeCb);
    });
    connect(applyFlipV, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setFlipVertical(this, static_cast<quint8>(flipVCombo_->currentData().toInt()), writeCb);
    });
    connect(applyExtSync, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setExternalSync(this, static_cast<quint8>(extSyncCombo_->currentData().toInt()), writeCb);
    });
}

// ---- 模式控制 ----

void IrPanel::setupModeControl(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("模式控制"), this);
    auto* form = new QFormLayout(grp);

    standbyCombo_ = new QComboBox(this);
    standbyCombo_->setObjectName(QStringLiteral("irStandbyCombo"));
    standbyCombo_->addItem(QString::fromUtf8("正常"), 0);
    standbyCombo_->addItem(QString::fromUtf8("待机"), 1);
    auto* applyStandby = new QPushButton(QString::fromUtf8("设待机"), this);
    form->addRow(QString::fromUtf8("待机"), makeValueButtonRow(standbyCombo_, applyStandby, this));

    autoCalibCombo_ = new QComboBox(this);
    autoCalibCombo_->setObjectName(QStringLiteral("irAutoCalibCombo"));
    autoCalibCombo_->addItem(QString::fromUtf8("关"), 0);
    autoCalibCombo_->addItem(QString::fromUtf8("开"), 1);
    auto* applyAutoCalib = new QPushButton(QString::fromUtf8("设上电校正"), this);
    form->addRow(QString::fromUtf8("上电自动校正\n(协议值反转)"),
                 makeValueButtonRow(autoCalibCombo_, applyAutoCalib, this));
    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };

    connect(applyStandby, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setStandby(this, static_cast<quint8>(standbyCombo_->currentData().toInt()), writeCb);
    });
    connect(applyAutoCalib, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setOnboardAutoCalibration(this, static_cast<quint8>(autoCalibCombo_->currentData().toInt()), writeCb);
    });
}

// ---- 查询与操作 ----

void IrPanel::setupQueries(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("查询与操作"), this);
    auto* vb = new QVBoxLayout(grp);

    actionStatusLabel_ = new QLabel("-", this);
    actionStatusLabel_->setWordWrap(true);
    actionStatusLabel_->setProperty("secondary", true);

    auto* forceShutterBtn = new QPushButton(QString::fromUtf8("强制制冷"), this);
    auto* versionBtn   = new QPushButton(QString::fromUtf8("版本"), this);
    auto* selfChkBtn   = new QPushButton(QString::fromUtf8("自检"), this);
    auto* coreTempBtn  = new QPushButton(QString::fromUtf8("读机芯温度"), this);
    auto* focusTempBtn = new QPushButton(QString::fromUtf8("读焦面温度"), this);
    auto* modIdBtn     = new QPushButton(QString::fromUtf8("读模组 ID"), this);
    auto* readMeanBtn  = new QPushButton(QString::fromUtf8("读均值"), this);
    auto* readCorrBtn  = new QPushButton(QString::fromUtf8("读校正档位"), this);
    auto* readBadBtn   = new QPushButton(QString::fromUtf8("读坏元数"), this);

    auto* actionRow = new QHBoxLayout();
    actionRow->addWidget(forceShutterBtn);
    vb->addLayout(actionRow);

    auto* versionReadout = makeReadoutLabel(QString::fromUtf8("版本"), this);
    auto* selfChkReadout = makeReadoutLabel(QString::fromUtf8("自检"), this);
    auto* coreTempReadout = makeReadoutLabel(QString::fromUtf8("机芯温度"), this);
    auto* focusTempReadout = makeReadoutLabel(QString::fromUtf8("焦面温度"), this);
    auto* modIdReadout = makeReadoutLabel(QString::fromUtf8("模组 ID"), this);
    auto* meanReadout = makeReadoutLabel(QString::fromUtf8("均值"), this);
    auto* corrReadout = makeReadoutLabel(QString::fromUtf8("校正档位"), this);
    auto* badReadout = makeReadoutLabel(QString::fromUtf8("坏元数"), this);

    vb->addWidget(makeReadoutButtonRow(versionReadout, versionBtn, this));
    vb->addWidget(makeReadoutButtonRow(selfChkReadout, selfChkBtn, this));
    vb->addWidget(makeReadoutButtonRow(coreTempReadout, coreTempBtn, this));
    vb->addWidget(makeReadoutButtonRow(focusTempReadout, focusTempBtn, this));
    vb->addWidget(makeReadoutButtonRow(modIdReadout, modIdBtn, this));
    vb->addWidget(makeReadoutButtonRow(meanReadout, readMeanBtn, this));
    vb->addWidget(makeReadoutButtonRow(corrReadout, readCorrBtn, this));
    vb->addWidget(makeReadoutButtonRow(badReadout, readBadBtn, this));
    vb->addWidget(actionStatusLabel_);
    root->addWidget(grp);

    auto readCb = [this](QLabel* label, const QString& title, const QString& unit = QString()) {
        return [this, label, title, unit](bool ok, const nlohmann::json& data, const QString& err) {
            updateReadoutLabel(label, title, ok, data, err, unit);
        };
    };

    connect(forceShutterBtn, &QPushButton::clicked, this, [this]() {
        dev_->ir()->forceShutter(this, [this](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, "IR", err);
            setActionStatus(ok ? "OK" : err);
        });
    });
    connect(versionBtn, &QPushButton::clicked, this, [this, readCb, versionReadout]() {
        dev_->ir()->getVersion(this, readCb(versionReadout, QString::fromUtf8("版本")));
    });
    connect(selfChkBtn, &QPushButton::clicked, this, [this, readCb, selfChkReadout]() {
        dev_->ir()->readSelfCheck(this, readCb(selfChkReadout, QString::fromUtf8("自检")));
    });
    connect(coreTempBtn, &QPushButton::clicked, this, [this, readCb, coreTempReadout]() {
        dev_->ir()->readCoreTemp(this, readCb(coreTempReadout, QString::fromUtf8("机芯温度"), QString::fromUtf8("℃")));
    });
    connect(focusTempBtn, &QPushButton::clicked, this, [this, readCb, focusTempReadout]() {
        dev_->ir()->readFocusPlaneTemp(this, readCb(focusTempReadout, QString::fromUtf8("焦面温度"), QString::fromUtf8("℃")));
    });
    connect(modIdBtn, &QPushButton::clicked, this, [this, readCb, modIdReadout]() {
        dev_->ir()->readModuleId(this, readCb(modIdReadout, QString::fromUtf8("模组 ID")));
    });
    connect(readMeanBtn, &QPushButton::clicked, this, [this, readCb, meanReadout]() {
        dev_->ir()->readMean(this, readCb(meanReadout, QString::fromUtf8("均值")));
    });
    connect(readCorrBtn, &QPushButton::clicked, this, [this, readCb, corrReadout]() {
        dev_->ir()->readCorrectionParamGear(this, readCb(corrReadout, QString::fromUtf8("校正档位")));
    });
    connect(readBadBtn, &QPushButton::clicked, this, [this, readCb, badReadout]() {
        dev_->ir()->readBadPixelCount(this, readCb(badReadout, QString::fromUtf8("坏元数")));
    });
}

// ---- 维护与校正 ----

void IrPanel::setupMaintenance(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("维护与校正"), this);
    auto* vb = new QVBoxLayout(grp);

    auto* warnLabel = new QLabel(QString::fromUtf8("以下操作会修改机芯持久化状态，请谨慎使用"), this);
    warnLabel->setStyleSheet("color: #c0392b; font-weight: bold;");
    warnLabel->setWordWrap(true);
    vb->addWidget(warnLabel);

    auto* form = new QFormLayout();

    maintUnlockCombo_ = new QComboBox(this);
    maintUnlockCombo_->setObjectName(QStringLiteral("irMaintenanceUnlockCombo"));
    maintUnlockCombo_->addItem(QString::fromUtf8("锁定"), 0);
    maintUnlockCombo_->addItem(QString::fromUtf8("解锁"), 1);
    auto* applyUnlock = new QPushButton(QString::fromUtf8("解锁/锁定"), this);
    form->addRow(QString::fromUtf8("维护锁"), makeValueButtonRow(maintUnlockCombo_, applyUnlock, this));

    maintExecNameCombo_ = new QComboBox(this);
    maintExecNameCombo_->setObjectName(QStringLiteral("irMaintenanceExecNameCombo"));
    maintExecNameCombo_->addItem("two_point_calib_p1", QString::fromUtf8("two_point_calib_p1"));
    maintExecNameCombo_->addItem("two_point_calib_p2", QString::fromUtf8("two_point_calib_p2"));
    maintExecNameCombo_->addItem("save_calib_params", QString::fromUtf8("save_calib_params"));
    maintExecNameCombo_->addItem("save_bad_pixel", QString::fromUtf8("save_bad_pixel"));
    maintExecNameCombo_->addItem("clear_k", QString::fromUtf8("clear_k"));
    maintExecNameCombo_->addItem("clear_b", QString::fromUtf8("clear_b"));
    maintExecNameCombo_->addItem("bad_pixel_search", QString::fromUtf8("bad_pixel_search"));

    maintExecValueSpin_ = new QSpinBox(this);
    maintExecValueSpin_->setObjectName(QStringLiteral("irMaintenanceExecValueSpin"));
    maintExecValueSpin_->setRange(0, 255);
    auto* applyMaintExec = new QPushButton(QString::fromUtf8("执行维护"), this);
    configureInlineField(maintExecNameCombo_);
    configureInlineField(maintExecValueSpin_);
    configureInlineButton(applyMaintExec);
    auto* maintExecRow = new QHBoxLayout();
    maintExecRow->setContentsMargins(0, 0, 0, 0);
    maintExecRow->setSpacing(6);
    maintExecRow->addWidget(maintExecNameCombo_, 1);
    maintExecRow->addWidget(new QLabel(QString::fromUtf8("参数"), this));
    maintExecRow->addWidget(maintExecValueSpin_);
    maintExecRow->addWidget(applyMaintExec);
    form->addRow(QString::fromUtf8("维护命令"), maintExecRow);

    vb->addLayout(form);

    // 分隔线
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    vb->addWidget(sep);

    auto* calibP1Btn  = new QPushButton(QString::fromUtf8("两点校正 P1"), this);
    auto* calibP2Btn  = new QPushButton(QString::fromUtf8("两点校正 P2"), this);
    auto* saveCalibBtn = new QPushButton(QString::fromUtf8("保存校正参数"), this);
    vb->addLayout(makeGrid2Col({calibP1Btn, calibP2Btn, saveCalibBtn}));

    clearKCombo_ = new QComboBox(this);
    clearKCombo_->setObjectName(QStringLiteral("irClearKCombo"));
    clearKCombo_->addItem(QString::fromUtf8("恢复"), 0);
    clearKCombo_->addItem(QString::fromUtf8("清除"), 1);
    clearBCombo_ = new QComboBox(this);
    clearBCombo_->setObjectName(QStringLiteral("irClearBCombo"));
    clearBCombo_->addItem(QString::fromUtf8("恢复"), 0);
    clearBCombo_->addItem(QString::fromUtf8("清除"), 1);

    auto* clearKLayout = new QHBoxLayout();
    clearKLayout->setContentsMargins(0, 0, 0, 0);
    clearKLayout->setSpacing(6);
    clearKLayout->addWidget(new QLabel(QString::fromUtf8("清除K:"), this));
    configureInlineField(clearKCombo_);
    clearKLayout->addWidget(clearKCombo_);
    auto* applyClearK = new QPushButton(QString::fromUtf8("设清除K"), this);
    configureInlineButton(applyClearK);
    clearKLayout->addWidget(applyClearK);
    vb->addLayout(clearKLayout);

    auto* clearBLayout = new QHBoxLayout();
    clearBLayout->setContentsMargins(0, 0, 0, 0);
    clearBLayout->setSpacing(6);
    clearBLayout->addWidget(new QLabel(QString::fromUtf8("清除B:"), this));
    configureInlineField(clearBCombo_);
    clearBLayout->addWidget(clearBCombo_);
    auto* applyClearB = new QPushButton(QString::fromUtf8("设清除B"), this);
    configureInlineButton(applyClearB);
    clearBLayout->addWidget(applyClearB);
    vb->addLayout(clearBLayout);

    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };
    auto actionCb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "IR", err);
        setActionStatus(ok ? "OK" : err);
    };
    auto showMaintenanceResult = [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            QMessageBox::warning(this, "IR", err);
            setActionStatus(err);
            return;
        }
        QString warning;
        auto it = data.find("warning");
        if (it != data.end() && it->is_string())
            warning = QString::fromStdString(it->get<std::string>());
        if (warning.isEmpty()) {
            QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
            setActionStatus("OK");
        } else {
            QMessageBox::warning(this, "IR", warning);
            setActionStatus(warning);
        }
    };

    connect(applyUnlock, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->maintenanceUnlock(this, static_cast<quint8>(maintUnlockCombo_->currentData().toInt()), writeCb);
    });
    connect(applyMaintExec, &QPushButton::clicked, this, [this, showMaintenanceResult]() {
        QString name = maintExecNameCombo_->currentData().toString();
        quint8 val = static_cast<quint8>(maintExecValueSpin_->value());
        dev_->ir()->maintenanceExec(this, name, val, showMaintenanceResult);
    });

    connect(calibP1Btn, &QPushButton::clicked, this, [this, actionCb]() {
        dev_->ir()->twoPointCalibP1(this, actionCb);
    });
    connect(calibP2Btn, &QPushButton::clicked, this, [this, actionCb]() {
        dev_->ir()->twoPointCalibP2(this, actionCb);
    });
    connect(saveCalibBtn, &QPushButton::clicked, this, [this, showMaintenanceResult]() {
        dev_->ir()->saveCalibParams(this, showMaintenanceResult);
    });
    connect(applyClearK, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->clearK(this, static_cast<quint8>(clearKCombo_->currentData().toInt()), writeCb);
    });
    connect(applyClearB, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->clearB(this, static_cast<quint8>(clearBCombo_->currentData().toInt()), writeCb);
    });
}

// ---- 坏元管理 ----

void IrPanel::setupBadPixel(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("坏元管理"), this);
    auto* form = new QFormLayout(grp);

    badPixelSearchCombo_ = new QComboBox(this);
    badPixelSearchCombo_->setObjectName(QStringLiteral("irBadPixelSearchCombo"));
    badPixelSearchCombo_->addItem(QString::fromUtf8("重复"), 1);
    badPixelSearchCombo_->addItem(QString::fromUtf8("新增"), 5);
    badPixelSearchCombo_->addItem(QString::fromUtf8("迭代"), 7);
    auto* applySearch = new QPushButton(QString::fromUtf8("坏元搜索"), this);
    form->addRow(QString::fromUtf8("坏元搜索"), makeValueButtonRow(badPixelSearchCombo_, applySearch, this));

    auto* applyPos = new QPushButton(QString::fromUtf8("设坏元位置"), this);
    {
        for (int i = 0; i < 4; ++i) {
            badPixelPosSpin_[i] = new QSpinBox(this);
            badPixelPosSpin_[i]->setObjectName(QStringLiteral("irBadPixelPos%1Spin").arg(i));
            badPixelPosSpin_[i]->setRange(0, 255);
        }
        auto* posRow = new QHBoxLayout();
        posRow->setContentsMargins(0, 0, 0, 0);
        posRow->setSpacing(6);
        configureInlineButton(applyPos);
        for (auto* spin : badPixelPosSpin_)
            configureCompactSpin(spin, 54);
        posRow->addLayout(makeGrid2Col({badPixelPosSpin_[0], badPixelPosSpin_[1],
                                        badPixelPosSpin_[2], badPixelPosSpin_[3]}));
        posRow->addWidget(applyPos);
        form->addRow(QString::fromUtf8("坏元位置 (4字节)"),
                     posRow);
    }

    auto* saveBadPixel = new QPushButton(QString::fromUtf8("保存坏元"), this);
    form->addRow(saveBadPixel);
    root->addWidget(grp);

    auto writeCb = [this](bool ok, const QString& err) {
        if (ok) QMessageBox::information(this, "IR", QString::fromUtf8("写入成功"));
        else QMessageBox::warning(this, "IR", err);
    };
    auto actionCb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "IR", err);
        setActionStatus(ok ? "OK" : err);
    };

    connect(applySearch, &QPushButton::clicked, this, [this, actionCb]() {
        dev_->ir()->badPixelSearch(this, static_cast<quint8>(badPixelSearchCombo_->currentData().toInt()), actionCb);
    });
    connect(applyPos, &QPushButton::clicked, this, [this, writeCb]() {
        quint8 pos[4];
        for (int i = 0; i < 4; ++i)
            pos[i] = static_cast<quint8>(badPixelPosSpin_[i]->value());
        dev_->ir()->setBadPixelPosition(this, pos, writeCb);
    });
    connect(saveBadPixel, &QPushButton::clicked, this, [this, actionCb]() {
        dev_->ir()->saveBadPixel(this, actionCb);
    });
}

// ---- helpers ----

void IrPanel::queryCurrentModel()
{
    if (!dev_) return;
    dev_->ir()->currentModel(this, [this](bool ok, const nlohmann::json& data, const QString& err) {
        if (!ok) {
            if (currentModelLabel_)
                currentModelLabel_->setText(QString::fromUtf8("当前机芯: ") + err);
            return;
        }

        currentModel_ = QString::fromStdString(data.value("model", std::string("legacy"))).toLower();
        if (currentModel_ != QStringLiteral("legacy") && currentModel_ != QStringLiteral("ci05"))
            currentModel_ = QStringLiteral("legacy");
        updateModelUi();
    });
}

bool IrPanel::isCi05Model() const
{
    return currentModel_ == QStringLiteral("ci05");
}

void IrPanel::updateModelUi()
{
    if (currentModelLabel_)
        currentModelLabel_->setText(QString::fromUtf8("当前机芯: ") + currentModel_);

    if (coreModeStack_ && legacyPage_ && ci05Page_)
        coreModeStack_->setCurrentWidget(isCi05Model() ? ci05Page_ : legacyPage_);
}

void IrPanel::loadSettings()
{
    QSettings s;
    const QString p = QStringLiteral("panels/ir/");

    brightSpin_->setValue(s.value(p + QStringLiteral("brightness"), brightSpin_->value()).toInt());
    contrastSpin_->setValue(s.value(p + QStringLiteral("contrast"), contrastSpin_->value()).toInt());
    ddeSpin_->setValue(s.value(p + QStringLiteral("dde"), ddeSpin_->value()).toInt());
    PanelSettings::setComboByData(abModeCombo_, s.value(p + QStringLiteral("abMode"), abModeCombo_->currentData()), 0);

    integSpin_->setValue(s.value(p + QStringLiteral("integration"), integSpin_->value()).toInt());
    PanelSettings::setComboByData(integModeCombo_, s.value(p + QStringLiteral("integrationMode"), integModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(gearModeCombo_, s.value(p + QStringLiteral("gearMode"), gearModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(gearSelectCombo_, s.value(p + QStringLiteral("gearSelect"), gearSelectCombo_->currentData()), 0);

    PanelSettings::setComboByData(imageTypeCombo_, s.value(p + QStringLiteral("imageType"), imageTypeCombo_->currentData()), 0);
    PanelSettings::setComboByData(testPatternCombo_, s.value(p + QStringLiteral("testPattern"), testPatternCombo_->currentData()), 0);
    PanelSettings::setComboByData(colorModeCombo_, s.value(p + QStringLiteral("colorMode"), colorModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(badPixelDispCombo_, s.value(p + QStringLiteral("badPixelDisplay"), badPixelDispCombo_->currentData()), 0);

    tempFilterChk_->setChecked(s.value(p + QStringLiteral("temporalFilterEnabled"), tempFilterChk_->isChecked()).toBool());
    tempFilterCoeffSpin_->setValue(s.value(p + QStringLiteral("temporalFilterCoeff"), tempFilterCoeffSpin_->value()).toInt());
    medianFilterChk_->setChecked(s.value(p + QStringLiteral("medianFilterEnabled"), medianFilterChk_->isChecked()).toBool());
    medianFilterCoeffSpin_->setValue(s.value(p + QStringLiteral("medianFilterCoeff"), medianFilterCoeffSpin_->value()).toInt());

    PanelSettings::setComboByData(flipHCombo_, s.value(p + QStringLiteral("flipH"), flipHCombo_->currentData()), 0);
    PanelSettings::setComboByData(flipVCombo_, s.value(p + QStringLiteral("flipV"), flipVCombo_->currentData()), 0);
    PanelSettings::setComboByData(extSyncCombo_, s.value(p + QStringLiteral("externalSync"), extSyncCombo_->currentData()), 0);
    PanelSettings::setComboByData(standbyCombo_, s.value(p + QStringLiteral("standby"), standbyCombo_->currentData()), 0);
    PanelSettings::setComboByData(autoCalibCombo_, s.value(p + QStringLiteral("autoCalibration"), autoCalibCombo_->currentData()), 0);

    PanelSettings::setComboByData(maintUnlockCombo_, s.value(p + QStringLiteral("maintenanceUnlock"), maintUnlockCombo_->currentData()), 0);
    PanelSettings::setComboByData(maintExecNameCombo_, s.value(p + QStringLiteral("maintenanceExecName"), maintExecNameCombo_->currentData()), 0);
    maintExecValueSpin_->setValue(s.value(p + QStringLiteral("maintenanceExecValue"), maintExecValueSpin_->value()).toInt());
    PanelSettings::setComboByData(clearKCombo_, s.value(p + QStringLiteral("clearK"), clearKCombo_->currentData()), 0);
    PanelSettings::setComboByData(clearBCombo_, s.value(p + QStringLiteral("clearB"), clearBCombo_->currentData()), 0);

    PanelSettings::setComboByData(badPixelSearchCombo_, s.value(p + QStringLiteral("badPixelSearch"), badPixelSearchCombo_->currentData()), 0);
    for (int i = 0; i < 4; ++i) {
        if (badPixelPosSpin_[i])
            badPixelPosSpin_[i]->setValue(s.value(p + QStringLiteral("badPixelPos%1").arg(i),
                                                  badPixelPosSpin_[i]->value()).toInt());
    }

    const QString c = p + QStringLiteral("ci05/");
    ci05BrightnessSpin_->setValue(s.value(c + QStringLiteral("brightness"), ci05BrightnessSpin_->value()).toInt());
    ci05ContrastSpin_->setValue(s.value(c + QStringLiteral("contrast"), ci05ContrastSpin_->value()).toInt());
    ci05OverallBrightnessSpin_->setValue(s.value(c + QStringLiteral("overallBrightness"), ci05OverallBrightnessSpin_->value()).toInt());
    ci05OverallContrastSpin_->setValue(s.value(c + QStringLiteral("overallContrast"), ci05OverallContrastSpin_->value()).toInt());
    ci05SharpnessSpin_->setValue(s.value(c + QStringLiteral("sharpness"), ci05SharpnessSpin_->value()).toInt());
    ci05FocusSpeedSpin_->setValue(s.value(c + QStringLiteral("focusSpeed"), ci05FocusSpeedSpin_->value()).toInt());
    ci05ZoomSpeedSpin_->setValue(s.value(c + QStringLiteral("zoomSpeed"), ci05ZoomSpeedSpin_->value()).toInt());
    ci05IntegrationMsSpin_->setValue(s.value(c + QStringLiteral("integrationMsX10"), ci05IntegrationMsSpin_->value()).toInt());
    ci05IntegrationMcSpin_->setValue(s.value(c + QStringLiteral("integrationMc"), ci05IntegrationMcSpin_->value()).toInt());
    ci05FrameRateSpin_->setValue(s.value(c + QStringLiteral("frameRateHzX100"), ci05FrameRateSpin_->value()).toInt());
    PanelSettings::setComboByData(ci05EzoomCombo_, s.value(c + QStringLiteral("ezoom"), ci05EzoomCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05FreezeCombo_, s.value(c + QStringLiteral("freeze"), ci05FreezeCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05AgcCombo_, s.value(c + QStringLiteral("agc"), ci05AgcCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05FovCombo_, s.value(c + QStringLiteral("fov"), ci05FovCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05MirrorCombo_, s.value(c + QStringLiteral("mirror"), ci05MirrorCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05PaletteCombo_, s.value(c + QStringLiteral("palette"), ci05PaletteCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05IntegrationGearCombo_, s.value(c + QStringLiteral("integrationGear"), ci05IntegrationGearCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05IntegrationAutoCombo_, s.value(c + QStringLiteral("integrationAuto"), ci05IntegrationAutoCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05BackgroundGearCombo_, s.value(c + QStringLiteral("backgroundGear"), ci05BackgroundGearCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05BackgroundAutoCombo_, s.value(c + QStringLiteral("backgroundAuto"), ci05BackgroundAutoCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05SyncModeCombo_, s.value(c + QStringLiteral("syncMode"), ci05SyncModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05Y8LevelCombo_, s.value(c + QStringLiteral("y8Level"), ci05Y8LevelCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05BootCompensationModeCombo_, s.value(c + QStringLiteral("bootCompensationMode"), ci05BootCompensationModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05GearSwitchCompensationModeCombo_, s.value(c + QStringLiteral("gearSwitchCompensationMode"), ci05GearSwitchCompensationModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05VideoSourceCombo_, s.value(c + QStringLiteral("videoSource"), ci05VideoSourceCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05ParamLineCombo_, s.value(c + QStringLiteral("paramLine"), ci05ParamLineCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05DigitalFormatCombo_, s.value(c + QStringLiteral("digitalFormat"), ci05DigitalFormatCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05TestPatternCombo_, s.value(c + QStringLiteral("testPattern"), ci05TestPatternCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05ImageModeCombo_, s.value(c + QStringLiteral("imageMode"), ci05ImageModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(ci05StatusOutputModeCombo_, s.value(c + QStringLiteral("statusOutputMode"), ci05StatusOutputModeCombo_->currentData()), 0);
    ci05TmodFilterSpin_->setValue(s.value(c + QStringLiteral("tmodFilter"), ci05TmodFilterSpin_->value()).toInt());
    ci05NtmFilterSpin_->setValue(s.value(c + QStringLiteral("ntmFilter"), ci05NtmFilterSpin_->value()).toInt());
    PanelSettings::setComboByData(ci05VerticalStripeRemovalCombo_, s.value(c + QStringLiteral("verticalStripeRemoval"), ci05VerticalStripeRemovalCombo_->currentData()), 0);
}

void IrPanel::saveSettings() const
{
    QSettings s;
    const QString p = QStringLiteral("panels/ir/");

    s.setValue(p + QStringLiteral("brightness"), brightSpin_->value());
    s.setValue(p + QStringLiteral("contrast"), contrastSpin_->value());
    s.setValue(p + QStringLiteral("dde"), ddeSpin_->value());
    s.setValue(p + QStringLiteral("abMode"), abModeCombo_->currentData());

    s.setValue(p + QStringLiteral("integration"), integSpin_->value());
    s.setValue(p + QStringLiteral("integrationMode"), integModeCombo_->currentData());
    s.setValue(p + QStringLiteral("gearMode"), gearModeCombo_->currentData());
    s.setValue(p + QStringLiteral("gearSelect"), gearSelectCombo_->currentData());

    s.setValue(p + QStringLiteral("imageType"), imageTypeCombo_->currentData());
    s.setValue(p + QStringLiteral("testPattern"), testPatternCombo_->currentData());
    s.setValue(p + QStringLiteral("colorMode"), colorModeCombo_->currentData());
    s.setValue(p + QStringLiteral("badPixelDisplay"), badPixelDispCombo_->currentData());

    s.setValue(p + QStringLiteral("temporalFilterEnabled"), tempFilterChk_->isChecked());
    s.setValue(p + QStringLiteral("temporalFilterCoeff"), tempFilterCoeffSpin_->value());
    s.setValue(p + QStringLiteral("medianFilterEnabled"), medianFilterChk_->isChecked());
    s.setValue(p + QStringLiteral("medianFilterCoeff"), medianFilterCoeffSpin_->value());

    s.setValue(p + QStringLiteral("flipH"), flipHCombo_->currentData());
    s.setValue(p + QStringLiteral("flipV"), flipVCombo_->currentData());
    s.setValue(p + QStringLiteral("externalSync"), extSyncCombo_->currentData());
    s.setValue(p + QStringLiteral("standby"), standbyCombo_->currentData());
    s.setValue(p + QStringLiteral("autoCalibration"), autoCalibCombo_->currentData());

    s.setValue(p + QStringLiteral("maintenanceUnlock"), maintUnlockCombo_->currentData());
    s.setValue(p + QStringLiteral("maintenanceExecName"), maintExecNameCombo_->currentData());
    s.setValue(p + QStringLiteral("maintenanceExecValue"), maintExecValueSpin_->value());
    s.setValue(p + QStringLiteral("clearK"), clearKCombo_->currentData());
    s.setValue(p + QStringLiteral("clearB"), clearBCombo_->currentData());

    s.setValue(p + QStringLiteral("badPixelSearch"), badPixelSearchCombo_->currentData());
    for (int i = 0; i < 4; ++i) {
        if (badPixelPosSpin_[i])
            s.setValue(p + QStringLiteral("badPixelPos%1").arg(i), badPixelPosSpin_[i]->value());
    }

    const QString c = p + QStringLiteral("ci05/");
    s.setValue(c + QStringLiteral("brightness"), ci05BrightnessSpin_->value());
    s.setValue(c + QStringLiteral("contrast"), ci05ContrastSpin_->value());
    s.setValue(c + QStringLiteral("overallBrightness"), ci05OverallBrightnessSpin_->value());
    s.setValue(c + QStringLiteral("overallContrast"), ci05OverallContrastSpin_->value());
    s.setValue(c + QStringLiteral("sharpness"), ci05SharpnessSpin_->value());
    s.setValue(c + QStringLiteral("focusSpeed"), ci05FocusSpeedSpin_->value());
    s.setValue(c + QStringLiteral("zoomSpeed"), ci05ZoomSpeedSpin_->value());
    s.setValue(c + QStringLiteral("integrationMsX10"), ci05IntegrationMsSpin_->value());
    s.setValue(c + QStringLiteral("integrationMc"), ci05IntegrationMcSpin_->value());
    s.setValue(c + QStringLiteral("frameRateHzX100"), ci05FrameRateSpin_->value());
    s.setValue(c + QStringLiteral("ezoom"), ci05EzoomCombo_->currentData());
    s.setValue(c + QStringLiteral("freeze"), ci05FreezeCombo_->currentData());
    s.setValue(c + QStringLiteral("agc"), ci05AgcCombo_->currentData());
    s.setValue(c + QStringLiteral("fov"), ci05FovCombo_->currentData());
    s.setValue(c + QStringLiteral("mirror"), ci05MirrorCombo_->currentData());
    s.setValue(c + QStringLiteral("palette"), ci05PaletteCombo_->currentData());
    s.setValue(c + QStringLiteral("integrationGear"), ci05IntegrationGearCombo_->currentData());
    s.setValue(c + QStringLiteral("integrationAuto"), ci05IntegrationAutoCombo_->currentData());
    s.setValue(c + QStringLiteral("backgroundGear"), ci05BackgroundGearCombo_->currentData());
    s.setValue(c + QStringLiteral("backgroundAuto"), ci05BackgroundAutoCombo_->currentData());
    s.setValue(c + QStringLiteral("syncMode"), ci05SyncModeCombo_->currentData());
    s.setValue(c + QStringLiteral("y8Level"), ci05Y8LevelCombo_->currentData());
    s.setValue(c + QStringLiteral("bootCompensationMode"), ci05BootCompensationModeCombo_->currentData());
    s.setValue(c + QStringLiteral("gearSwitchCompensationMode"), ci05GearSwitchCompensationModeCombo_->currentData());
    s.setValue(c + QStringLiteral("videoSource"), ci05VideoSourceCombo_->currentData());
    s.setValue(c + QStringLiteral("paramLine"), ci05ParamLineCombo_->currentData());
    s.setValue(c + QStringLiteral("digitalFormat"), ci05DigitalFormatCombo_->currentData());
    s.setValue(c + QStringLiteral("testPattern"), ci05TestPatternCombo_->currentData());
    s.setValue(c + QStringLiteral("imageMode"), ci05ImageModeCombo_->currentData());
    s.setValue(c + QStringLiteral("statusOutputMode"), ci05StatusOutputModeCombo_->currentData());
    s.setValue(c + QStringLiteral("tmodFilter"), ci05TmodFilterSpin_->value());
    s.setValue(c + QStringLiteral("ntmFilter"), ci05NtmFilterSpin_->value());
    s.setValue(c + QStringLiteral("verticalStripeRemoval"), ci05VerticalStripeRemovalCombo_->currentData());
}

void IrPanel::connectSettingSignals()
{
    auto connectSpin = [this](QSpinBox* spin) {
        if (!spin) return;
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    };
    auto connectCombo = [this](QComboBox* combo) {
        if (!combo) return;
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { saveSettings(); });
    };
    auto connectCheck = [this](QCheckBox* check) {
        if (!check) return;
        connect(check, &QCheckBox::toggled, this, [this](bool) { saveSettings(); });
    };

    connectSpin(brightSpin_);
    connectSpin(contrastSpin_);
    connectSpin(ddeSpin_);
    connectCombo(abModeCombo_);
    connectSpin(integSpin_);
    connectCombo(integModeCombo_);
    connectCombo(gearModeCombo_);
    connectCombo(gearSelectCombo_);
    connectCombo(imageTypeCombo_);
    connectCombo(testPatternCombo_);
    connectCombo(colorModeCombo_);
    connectCombo(badPixelDispCombo_);
    connectCheck(tempFilterChk_);
    connectSpin(tempFilterCoeffSpin_);
    connectCheck(medianFilterChk_);
    connectSpin(medianFilterCoeffSpin_);
    connectCombo(flipHCombo_);
    connectCombo(flipVCombo_);
    connectCombo(extSyncCombo_);
    connectCombo(standbyCombo_);
    connectCombo(autoCalibCombo_);
    connectCombo(maintUnlockCombo_);
    connectCombo(maintExecNameCombo_);
    connectSpin(maintExecValueSpin_);
    connectCombo(clearKCombo_);
    connectCombo(clearBCombo_);
    connectCombo(badPixelSearchCombo_);
    for (QSpinBox* spin : badPixelPosSpin_) {
        if (spin) connectSpin(spin);
    }

    connectSpin(ci05BrightnessSpin_);
    connectSpin(ci05ContrastSpin_);
    connectSpin(ci05OverallBrightnessSpin_);
    connectSpin(ci05OverallContrastSpin_);
    connectSpin(ci05SharpnessSpin_);
    connectSpin(ci05FocusSpeedSpin_);
    connectSpin(ci05ZoomSpeedSpin_);
    connectSpin(ci05IntegrationMsSpin_);
    connectSpin(ci05IntegrationMcSpin_);
    connectSpin(ci05FrameRateSpin_);
    connectCombo(ci05EzoomCombo_);
    connectCombo(ci05FreezeCombo_);
    connectCombo(ci05AgcCombo_);
    connectCombo(ci05FovCombo_);
    connectCombo(ci05MirrorCombo_);
    connectCombo(ci05PaletteCombo_);
    connectCombo(ci05IntegrationGearCombo_);
    connectCombo(ci05IntegrationAutoCombo_);
    connectCombo(ci05BackgroundGearCombo_);
    connectCombo(ci05BackgroundAutoCombo_);
    connectCombo(ci05SyncModeCombo_);
    connectCombo(ci05Y8LevelCombo_);
    connectCombo(ci05BootCompensationModeCombo_);
    connectCombo(ci05GearSwitchCompensationModeCombo_);
    connectCombo(ci05VideoSourceCombo_);
    connectCombo(ci05ParamLineCombo_);
    connectCombo(ci05DigitalFormatCombo_);
    connectCombo(ci05TestPatternCombo_);
    connectCombo(ci05ImageModeCombo_);
    connectCombo(ci05StatusOutputModeCombo_);
    connectSpin(ci05TmodFilterSpin_);
    connectSpin(ci05NtmFilterSpin_);
    connectCombo(ci05VerticalStripeRemovalCombo_);
}

void IrPanel::setActionStatus(const QString& text)
{
    if (actionStatusLabel_)
        actionStatusLabel_->setText(text);
}

void IrPanel::updateReadoutLabel(QLabel* label, const QString& title, bool ok, const nlohmann::json& data,
                                 const QString& err, const QString& unit)
{
    if (!label) return;
    if (!ok) {
        label->setText(title + ": " + err);
        return;
    }
    auto it = data.find("value");
    if (it == data.end()) {
        label->setText(title + ": " + QString::fromUtf8("<无数据>"));
        return;
    }

    QString text = jsonValueToText(*it);
    if (!unit.isEmpty() && text != "-")
        text += " " + unit;
    label->setText(title + ": " + text);
}

QString IrPanel::jsonValueToText(const nlohmann::json& value)
{
    if (value.is_number_unsigned()) {
        return QString::number(value.get<unsigned long long>());
    } else if (value.is_number_integer()) {
        return QString::number(value.get<long long>());
    } else if (value.is_number_float()) {
        return QString::number(value.get<double>());
    } else if (value.is_string()) {
        return QString::fromStdString(value.get<std::string>());
    } else if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    } else if (value.is_array()) {
        return bytesToHex(value);
    }
    return QString::fromStdString(value.dump());
}

QString IrPanel::bytesToHex(const nlohmann::json& arr)
{
    if (!arr.is_array()) return "-";
    QString s;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (!s.isEmpty()) s += " ";
        s += QString("%1").arg(arr[i].get<int>(), 2, 16, QChar('0')).toUpper();
    }
    return s;
}

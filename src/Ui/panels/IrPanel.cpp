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

    setupImageParams(root);
    setupIntegration(root);
    setupImageDisplay(root);
    setupFilters(root);
    setupFlipSync(root);
    setupModeControl(root);
    setupQueries(root);
    setupMaintenance(root);
    setupBadPixel(root);

    loadSettings();
    connectSettingSignals();

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
    auto* applyContrast = new QPushButton(QString::fromUtf8("设对比度"), this);
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
        dev_->ir()->setBrightness(this, static_cast<quint8>(brightSpin_->value()), writeCb);
    });
    connect(applyContrast, &QPushButton::clicked, this, [this, writeCb]() {
        dev_->ir()->setContrast(this, static_cast<quint8>(contrastSpin_->value()), writeCb);
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

    auto* calibBtn     = new QPushButton(QString::fromUtf8("触发标定"), this);
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
    actionRow->addWidget(calibBtn);
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

    connect(calibBtn, &QPushButton::clicked, this, [this]() {
        dev_->ir()->triggerCalibration(this, [this](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, "IR", err);
            setActionStatus(ok ? "OK" : err);
        });
    });
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
}

void IrPanel::connectSettingSignals()
{
    auto connectSpin = [this](QSpinBox* spin) {
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    };
    auto connectCombo = [this](QComboBox* combo) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { saveSettings(); });
    };
    auto connectCheck = [this](QCheckBox* check) {
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

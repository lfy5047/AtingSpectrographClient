#include "IrPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QMessageBox>

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
    setupRawCmd(root);

    root->addStretch();
}

// ---- 亮度 / 对比度 / DDE / AB 模式 ----

void IrPanel::setupImageParams(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("亮度 / 对比度 / DDE"), this);
    auto* form = new QFormLayout(grp);

    auto makeSliderRow = [this](QSlider*& slider, QSpinBox*& spin, int mx) {
        auto* row = new QHBoxLayout();
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, mx);
        spin = new QSpinBox(this);
        spin->setRange(0, mx);
        connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
        row->addWidget(slider, 1);
        row->addWidget(spin);
        return row;
    };

    form->addRow(QString::fromUtf8("亮度"), makeSliderRow(brightSlider_, brightSpin_, 255));
    form->addRow(QString::fromUtf8("对比度"), makeSliderRow(contrastSlider_, contrastSpin_, 255));
    form->addRow(QString::fromUtf8("DDE"), makeSliderRow(ddeSlider_, ddeSpin_, 15));

    abModeCombo_ = new QComboBox(this);
    abModeCombo_->addItem(QString::fromUtf8("手动"), 0);
    abModeCombo_->addItem(QString::fromUtf8("自动"), 1);
    form->addRow(QString::fromUtf8("AB 模式"), abModeCombo_);

    auto* applyBright   = new QPushButton(QString::fromUtf8("设亮度"), this);
    auto* applyContrast = new QPushButton(QString::fromUtf8("设对比"), this);
    auto* applyDde      = new QPushButton(QString::fromUtf8("设 DDE"), this);
    auto* applyAbMode   = new QPushButton(QString::fromUtf8("设 AB 模式"), this);
    form->addRow(makeGrid2Col({applyBright, applyContrast, applyDde, applyAbMode}));
    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyBright, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setBrightness(this, static_cast<quint8>(brightSpin_->value()), warn("IR"));
    });
    connect(applyContrast, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setContrast(this, static_cast<quint8>(contrastSpin_->value()), warn("IR"));
    });
    connect(applyDde, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setDde(this, static_cast<quint8>(ddeSpin_->value()), warn("IR"));
    });
    connect(applyAbMode, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setAbMode(this, static_cast<quint8>(abModeCombo_->currentData().toInt()), warn("IR"));
    });
}

// ---- 积分时间 ----

void IrPanel::setupIntegration(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("积分时间"), this);
    auto* form = new QFormLayout(grp);

    integSpin_ = new QSpinBox(this);
    integSpin_->setRange(0, 65535);
    form->addRow(QString::fromUtf8("积分时间"), integSpin_);

    integModeCombo_ = new QComboBox(this);
    integModeCombo_->addItem(QString::fromUtf8("自动积分"), 0);
    integModeCombo_->addItem(QString::fromUtf8("手动积分"), 1);
    form->addRow(QString::fromUtf8("积分模式"), integModeCombo_);

    gearModeCombo_ = new QComboBox(this);
    gearModeCombo_->addItem(QString::fromUtf8("手动切换"), 0);
    gearModeCombo_->addItem(QString::fromUtf8("自动切换"), 1);
    form->addRow(QString::fromUtf8("档位模式"), gearModeCombo_);

    gearSelectCombo_ = new QComboBox(this);
    for (int i = 0; i < 8; ++i)
        gearSelectCombo_->addItem(QString::number(i), i);
    form->addRow(QString::fromUtf8("档位选择"), gearSelectCombo_);

    auto* applyInteg       = new QPushButton(QString::fromUtf8("设积分"), this);
    auto* applyIntegMode   = new QPushButton(QString::fromUtf8("设积分模式"), this);
    auto* applyGearMode    = new QPushButton(QString::fromUtf8("设档位模式"), this);
    auto* applyGear        = new QPushButton(QString::fromUtf8("选积分档"), this);
    auto* queryIntBtn      = new QPushButton(QString::fromUtf8("查询积分时间"), this);

    form->addRow(makeGrid2Col({applyInteg, applyIntegMode, applyGearMode}));

    auto* row2 = new QHBoxLayout();
    row2->addWidget(applyGear);
    row2->addWidget(queryIntBtn);
    form->addRow(row2);

    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyInteg, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setIntegration(this, static_cast<quint16>(integSpin_->value()), warn("IR"));
    });
    connect(applyIntegMode, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setManualIntegration(this, static_cast<quint8>(integModeCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyGearMode, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setIntegrationGearMode(this, static_cast<quint8>(gearModeCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyGear, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->selectIntegrationGear(this, static_cast<quint8>(gearSelectCombo_->currentData().toInt()), warn("IR"));
    });
    connect(queryIntBtn, &QPushButton::clicked, this, [this]() {
        dev_->ir()->queryIntegrationTime(this, [this](bool ok, const nlohmann::json& data, const QString& err) {
            showResult(QString::fromUtf8("积分时间"), ok, data, err);
        });
    });
}

// ---- 图像显示 ----

void IrPanel::setupImageDisplay(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("图像显示"), this);
    auto* form = new QFormLayout(grp);

    imageTypeCombo_ = new QComboBox(this);
    imageTypeCombo_->addItem(QString::fromUtf8("增强 8bit"), 0);
    imageTypeCombo_->addItem(QString::fromUtf8("14bit 原始"), 1);
    imageTypeCombo_->addItem(QString::fromUtf8("14bit 预处理"), 2);
    imageTypeCombo_->addItem(QString::fromUtf8("测试图"), 3);
    form->addRow(QString::fromUtf8("图像类型"), imageTypeCombo_);

    testPatternCombo_ = new QComboBox(this);
    testPatternCombo_->addItem(QString::fromUtf8("竖灰阶"), 0);
    testPatternCombo_->addItem(QString::fromUtf8("横灰阶"), 1);
    testPatternCombo_->addItem(QString::fromUtf8("棋盘格"), 2);
    form->addRow(QString::fromUtf8("测试图"), testPatternCombo_);

    colorModeCombo_ = new QComboBox(this);
    colorModeCombo_->addItem(QString::fromUtf8("白热"), 0);
    colorModeCombo_->addItem(QString::fromUtf8("黑热"), 1);
    form->addRow(QString::fromUtf8("彩色模式"), colorModeCombo_);

    badPixelDispCombo_ = new QComboBox(this);
    badPixelDispCombo_->addItem(QString::fromUtf8("正常"), 0);
    badPixelDispCombo_->addItem(QString::fromUtf8("高亮"), 1);
    form->addRow(QString::fromUtf8("坏元显示"), badPixelDispCombo_);

    auto* applyImageType    = new QPushButton(QString::fromUtf8("设图像类型"), this);
    auto* applyTestPattern  = new QPushButton(QString::fromUtf8("设测试图"), this);
    auto* applyColorMode    = new QPushButton(QString::fromUtf8("设彩色模式"), this);
    auto* applyBadPixelDisp = new QPushButton(QString::fromUtf8("设坏元显示"), this);
    form->addRow(makeGrid2Col({applyImageType, applyTestPattern, applyColorMode, applyBadPixelDisp}));
    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyImageType, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setImageType(this, static_cast<quint8>(imageTypeCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyTestPattern, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setTestPattern(this, static_cast<quint8>(testPatternCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyColorMode, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setColorMode(this, static_cast<quint8>(colorModeCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyBadPixelDisp, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setBadPixelDisplayMode(this, static_cast<quint8>(badPixelDispCombo_->currentData().toInt()), warn("IR"));
    });
}

// ---- 滤波 ----

void IrPanel::setupFilters(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("滤波"), this);
    auto* form = new QFormLayout(grp);

    {
        auto* row = new QHBoxLayout();
        tempFilterChk_ = new QCheckBox(QString::fromUtf8("启用"), this);
        tempFilterCoeffSpin_ = new QSpinBox(this);
        tempFilterCoeffSpin_->setRange(1, 15);
        tempFilterCoeffSpin_->setValue(1);
        row->addWidget(tempFilterChk_);
        row->addWidget(new QLabel(QString::fromUtf8("系数 1-15"), this));
        row->addWidget(tempFilterCoeffSpin_);
        row->addStretch();
        form->addRow(QString::fromUtf8("时域滤波"), row);
    }

    {
        auto* row = new QHBoxLayout();
        medianFilterChk_ = new QCheckBox(QString::fromUtf8("启用"), this);
        medianFilterCoeffSpin_ = new QSpinBox(this);
        medianFilterCoeffSpin_->setRange(10, 127);
        medianFilterCoeffSpin_->setValue(10);
        row->addWidget(medianFilterChk_);
        row->addWidget(new QLabel(QString::fromUtf8("系数 10-127"), this));
        row->addWidget(medianFilterCoeffSpin_);
        row->addStretch();
        form->addRow(QString::fromUtf8("中值滤波"), row);
    }

    auto* applyTempFilter   = new QPushButton(QString::fromUtf8("设时域滤波"), this);
    auto* applyMedianFilter = new QPushButton(QString::fromUtf8("设中值滤波"), this);
    auto* row = new QHBoxLayout();
    row->addWidget(applyTempFilter);
    row->addWidget(applyMedianFilter);
    form->addRow(row);
    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyTempFilter, &QPushButton::clicked, this, [this, warn]() {
        bool en = tempFilterChk_->isChecked();
        quint8 coeff = static_cast<quint8>(tempFilterCoeffSpin_->value());
        dev_->ir()->setTemporalFilter(this, en, coeff, warn("IR"));
    });
    connect(applyMedianFilter, &QPushButton::clicked, this, [this, warn]() {
        bool en = medianFilterChk_->isChecked();
        quint8 coeff = static_cast<quint8>(medianFilterCoeffSpin_->value());
        dev_->ir()->setMedianFilter(this, en, coeff, warn("IR"));
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
    form->addRow(QString::fromUtf8("左右翻转"), flipHCombo_);

    flipVCombo_ = makeOnOffCombo();
    form->addRow(QString::fromUtf8("上下翻转"), flipVCombo_);

    extSyncCombo_ = makeOnOffCombo();
    form->addRow(QString::fromUtf8("外同步"), extSyncCombo_);

    auto* applyFlipH   = new QPushButton(QString::fromUtf8("设左右翻转"), this);
    auto* applyFlipV   = new QPushButton(QString::fromUtf8("设上下翻转"), this);
    auto* applyExtSync = new QPushButton(QString::fromUtf8("设外同步"), this);
    form->addRow(makeGrid2Col({applyFlipH, applyFlipV, applyExtSync}));
    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyFlipH, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setFlipHorizontal(this, static_cast<quint8>(flipHCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyFlipV, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setFlipVertical(this, static_cast<quint8>(flipVCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyExtSync, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setExternalSync(this, static_cast<quint8>(extSyncCombo_->currentData().toInt()), warn("IR"));
    });
}

// ---- 模式控制 ----

void IrPanel::setupModeControl(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("模式控制"), this);
    auto* form = new QFormLayout(grp);

    standbyCombo_ = new QComboBox(this);
    standbyCombo_->addItem(QString::fromUtf8("正常"), 0);
    standbyCombo_->addItem(QString::fromUtf8("待机"), 1);
    form->addRow(QString::fromUtf8("待机"), standbyCombo_);

    autoCalibCombo_ = new QComboBox(this);
    autoCalibCombo_->addItem(QString::fromUtf8("关"), 0);
    autoCalibCombo_->addItem(QString::fromUtf8("开"), 1);
    form->addRow(QString::fromUtf8("上电自动校正\n(协议值反转)"), autoCalibCombo_);

    auto* applyStandby   = new QPushButton(QString::fromUtf8("设待机"), this);
    auto* applyAutoCalib = new QPushButton(QString::fromUtf8("设上电校正"), this);
    auto* row = new QHBoxLayout();
    row->addWidget(applyStandby);
    row->addWidget(applyAutoCalib);
    form->addRow(row);
    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyStandby, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setStandby(this, static_cast<quint8>(standbyCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyAutoCalib, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->setOnboardAutoCalibration(this, static_cast<quint8>(autoCalibCombo_->currentData().toInt()), warn("IR"));
    });
}

// ---- 查询与操作 ----

void IrPanel::setupQueries(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("查询与操作"), this);
    auto* vb = new QVBoxLayout(grp);

    resultLabel_ = new QLabel("-", this);
    resultLabel_->setWordWrap(true);
    resultLabel_->setProperty("secondary", true);

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

    vb->addWidget(calibBtn);
    vb->addWidget(forceShutterBtn);
    vb->addWidget(versionBtn);
    vb->addWidget(selfChkBtn);
    vb->addWidget(coreTempBtn);
    vb->addWidget(focusTempBtn);
    vb->addWidget(modIdBtn);
    vb->addWidget(readMeanBtn);
    vb->addWidget(readCorrBtn);
    vb->addWidget(readBadBtn);
    vb->addWidget(resultLabel_);
    root->addWidget(grp);

    auto showCb = [this](const QString& title) {
        return [this, title](bool ok, const nlohmann::json& data, const QString& err) {
            showResult(title, ok, data, err);
        };
    };

    connect(calibBtn, &QPushButton::clicked, this, [this]() {
        dev_->ir()->triggerCalibration(this, [this](bool ok, const QString& err) {
            resultLabel_->setText(ok ? "OK" : err);
        });
    });
    connect(forceShutterBtn, &QPushButton::clicked, this, [this]() {
        dev_->ir()->forceShutter(this, [this](bool ok, const QString& err) {
            resultLabel_->setText(ok ? "OK" : err);
        });
    });
    connect(versionBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->getVersion(this, showCb(QString::fromUtf8("版本")));
    });
    connect(selfChkBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readSelfCheck(this, showCb(QString::fromUtf8("自检")));
    });
    connect(coreTempBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readCoreTemp(this, showCb(QString::fromUtf8("机芯温度")));
    });
    connect(focusTempBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readFocusPlaneTemp(this, showCb(QString::fromUtf8("焦面温度")));
    });
    connect(modIdBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readModuleId(this, showCb(QString::fromUtf8("模组 ID")));
    });
    connect(readMeanBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readMean(this, showCb(QString::fromUtf8("均值")));
    });
    connect(readCorrBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readCorrectionParamGear(this, showCb(QString::fromUtf8("校正档位")));
    });
    connect(readBadBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->ir()->readBadPixelCount(this, showCb(QString::fromUtf8("坏元数")));
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
    maintUnlockCombo_->addItem(QString::fromUtf8("锁定"), 0);
    maintUnlockCombo_->addItem(QString::fromUtf8("解锁"), 1);
    form->addRow(QString::fromUtf8("维护锁"), maintUnlockCombo_);

    maintExecNameCombo_ = new QComboBox(this);
    maintExecNameCombo_->addItem("two_point_calib_p1", QString::fromUtf8("two_point_calib_p1"));
    maintExecNameCombo_->addItem("two_point_calib_p2", QString::fromUtf8("two_point_calib_p2"));
    maintExecNameCombo_->addItem("save_calib_params", QString::fromUtf8("save_calib_params"));
    maintExecNameCombo_->addItem("save_bad_pixel", QString::fromUtf8("save_bad_pixel"));
    maintExecNameCombo_->addItem("clear_k", QString::fromUtf8("clear_k"));
    maintExecNameCombo_->addItem("clear_b", QString::fromUtf8("clear_b"));
    maintExecNameCombo_->addItem("bad_pixel_search", QString::fromUtf8("bad_pixel_search"));
    form->addRow(QString::fromUtf8("维护命令"), maintExecNameCombo_);

    maintExecValueSpin_ = new QSpinBox(this);
    maintExecValueSpin_->setRange(0, 255);
    form->addRow(QString::fromUtf8("参数值"), maintExecValueSpin_);

    vb->addLayout(form);

    auto* applyUnlock  = new QPushButton(QString::fromUtf8("解锁/锁定"), this);
    auto* applyMaintExec = new QPushButton(QString::fromUtf8("执行维护"), this);
    auto* row1 = new QHBoxLayout();
    row1->addWidget(applyUnlock);
    row1->addWidget(applyMaintExec);
    vb->addLayout(row1);

    // 分隔线
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    vb->addWidget(sep);

    auto* calibP1Btn  = new QPushButton(QString::fromUtf8("两点校正 P1"), this);
    auto* calibP2Btn  = new QPushButton(QString::fromUtf8("两点校正 P2"), this);
    auto* saveCalibBtn = new QPushButton(QString::fromUtf8("保存校正参数"), this);
    vb->addLayout(makeGrid2Col({calibP1Btn, calibP2Btn, saveCalibBtn}));

    clearKCombo_ = new QComboBox(this);
    clearKCombo_->addItem(QString::fromUtf8("恢复"), 0);
    clearKCombo_->addItem(QString::fromUtf8("清除"), 1);
    clearBCombo_ = new QComboBox(this);
    clearBCombo_->addItem(QString::fromUtf8("恢复"), 0);
    clearBCombo_->addItem(QString::fromUtf8("清除"), 1);

    auto* clearKLayout = new QHBoxLayout();
    clearKLayout->addWidget(new QLabel(QString::fromUtf8("清除K:"), this));
    clearKLayout->addWidget(clearKCombo_);
    auto* applyClearK = new QPushButton(QString::fromUtf8("设清除K"), this);
    clearKLayout->addWidget(applyClearK);
    vb->addLayout(clearKLayout);

    auto* clearBLayout = new QHBoxLayout();
    clearBLayout->addWidget(new QLabel(QString::fromUtf8("清除B:"), this));
    clearBLayout->addWidget(clearBCombo_);
    auto* applyClearB = new QPushButton(QString::fromUtf8("设清除B"), this);
    clearBLayout->addWidget(applyClearB);
    vb->addLayout(clearBLayout);

    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applyUnlock, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->maintenanceUnlock(this, static_cast<quint8>(maintUnlockCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyMaintExec, &QPushButton::clicked, this, [this, warn]() {
        QString name = maintExecNameCombo_->currentData().toString();
        quint8 val = static_cast<quint8>(maintExecValueSpin_->value());
        dev_->ir()->maintenanceExec(this, name, val, warn("IR"));
    });

    connect(calibP1Btn, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->twoPointCalibP1(this, warn("IR"));
    });
    connect(calibP2Btn, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->twoPointCalibP2(this, warn("IR"));
    });
    connect(saveCalibBtn, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->saveCalibParams(this, warn("IR"));
    });
    connect(applyClearK, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->clearK(this, static_cast<quint8>(clearKCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyClearB, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->clearB(this, static_cast<quint8>(clearBCombo_->currentData().toInt()), warn("IR"));
    });
}

// ---- 坏元管理 ----

void IrPanel::setupBadPixel(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("坏元管理"), this);
    auto* form = new QFormLayout(grp);

    badPixelSearchCombo_ = new QComboBox(this);
    badPixelSearchCombo_->addItem(QString::fromUtf8("重复"), 1);
    badPixelSearchCombo_->addItem(QString::fromUtf8("新增"), 5);
    badPixelSearchCombo_->addItem(QString::fromUtf8("迭代"), 7);
    form->addRow(QString::fromUtf8("坏元搜索"), badPixelSearchCombo_);

    {
        for (int i = 0; i < 4; ++i) {
            badPixelPosSpin_[i] = new QSpinBox(this);
            badPixelPosSpin_[i]->setRange(0, 255);
        }
        form->addRow(QString::fromUtf8("坏元位置 (4字节)"),
                     makeGrid2Col({badPixelPosSpin_[0], badPixelPosSpin_[1],
                                   badPixelPosSpin_[2], badPixelPosSpin_[3]}));
    }

    auto* applySearch   = new QPushButton(QString::fromUtf8("坏元搜索"), this);
    auto* applyPos      = new QPushButton(QString::fromUtf8("设坏元位置"), this);
    auto* saveBadPixel  = new QPushButton(QString::fromUtf8("保存坏元"), this);
    form->addRow(makeGrid2Col({applySearch, applyPos, saveBadPixel}));
    root->addWidget(grp);

    auto warn = [this](const QString& title) {
        return [this, title](bool ok, const QString& err) {
            if (!ok) QMessageBox::warning(this, title, err);
        };
    };

    connect(applySearch, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->badPixelSearch(this, static_cast<quint8>(badPixelSearchCombo_->currentData().toInt()), warn("IR"));
    });
    connect(applyPos, &QPushButton::clicked, this, [this, warn]() {
        quint8 pos[4];
        for (int i = 0; i < 4; ++i)
            pos[i] = static_cast<quint8>(badPixelPosSpin_[i]->value());
        dev_->ir()->setBadPixelPosition(this, pos, warn("IR"));
    });
    connect(saveBadPixel, &QPushButton::clicked, this, [this, warn]() {
        dev_->ir()->saveBadPixel(this, warn("IR"));
    });
}

// ---- 原始命令 ----

void IrPanel::setupRawCmd(QVBoxLayout* root)
{
    auto* grp = new QGroupBox(QString::fromUtf8("原始命令"), this);
    auto* form = new QFormLayout(grp);

    rawCmdEdit_ = new QLineEdit("02", this);
    rawDataEdit_ = new QLineEdit("01", this);
    rawLenSpin_ = new QSpinBox(this);
    rawLenSpin_->setRange(0, 255);
    rawLenSpin_->setValue(1);
    rawResultLabel_ = new QLabel("-", this);
    rawResultLabel_->setWordWrap(true);
    rawResultLabel_->setProperty("secondary", true);

    form->addRow("CMD (hex)", rawCmdEdit_);
    form->addRow("DATA (hex)", rawDataEdit_);
    form->addRow("Readback Len", rawLenSpin_);

    auto* sendBtn = new QPushButton(QString::fromUtf8("发送"), this);
    form->addRow(sendBtn);
    form->addRow(rawResultLabel_);
    root->addWidget(grp);

    connect(sendBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        quint8 cmd = static_cast<quint8>(rawCmdEdit_->text().toUInt(&ok, 16));
        if (!ok) { QMessageBox::warning(this, "IR", "Invalid CMD hex"); return; }
        QByteArray data = QByteArray::fromHex(rawDataEdit_->text().toLatin1());
        quint8 len = static_cast<quint8>(rawLenSpin_->value());
        dev_->ir()->sendRaw(this, cmd, data, len, [this](bool ok2, const nlohmann::json& d, const QString& err) {
            if (ok2)
                rawResultLabel_->setText("cmd=" + QString::number(d.value("cmd", 0)) + " data=" + bytesToHex(d["data"]));
            else
                rawResultLabel_->setText(err);
        });
    });
}

// ---- helpers ----

void IrPanel::showResult(const QString& title, bool ok, const nlohmann::json& data, const QString& err)
{
    if (ok)
        resultLabel_->setText(title + ": " + bytesToHex(data["data"]));
    else
        resultLabel_->setText(title + ": " + err);
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

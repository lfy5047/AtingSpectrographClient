#include "TempControlPanel.h"

#include "DeviceClient.h"

#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const QString kSettingsPrefix = QStringLiteral("panels/tempControl/");

QString boolStateText(bool enabled)
{
    return enabled ? QString::fromUtf8("已打开") : QString::fromUtf8("已关闭");
}

QLabel* makeReadout(QWidget* parent)
{
    auto* label = new QLabel(QStringLiteral("-"), parent);
    label->setProperty("readout", true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

void addButtonRow(QFormLayout* form, const QList<QPushButton*>& buttons)
{
    auto* row = new QHBoxLayout();
    for (auto* button : buttons) {
        row->addWidget(button);
    }
    form->addRow(row);
}

}

TempControlPanel::TempControlPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent)
    , dev_(dev)
{
    setupUi();
    loadSettings();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(1000);
    connect(refreshTimer_, &QTimer::timeout, this, &TempControlPanel::refreshStatus);

    connect(dev_, &DeviceClient::connectionChanged, this, [this](bool connected, const QString&) {
        if (connected) {
            refreshStatus();
            refreshTimer_->start();
        } else {
            refreshTimer_->stop();
            setStatusUnavailable(QString::fromUtf8("未连接"));
        }
    });

    connect(targetTemperatureSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { saveSettings(); });
    connect(keyCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { saveSettings(); });
    connect(advancedValueEdit_, &QLineEdit::textChanged, this, [this](const QString&) { saveSettings(); });
    connect(moduleEdit_, &QLineEdit::textChanged, this, [this](const QString&) { saveSettings(); });
    connect(paramEdit_, &QLineEdit::textChanged, this, [this](const QString&) { saveSettings(); });
    connect(rawCommandEdit_, &QLineEdit::textChanged, this, [this](const QString&) { saveSettings(); });

    connect(setTargetButton_, &QPushButton::clicked, this, [this]() {
        const double value = targetTemperatureSpin_->value();
        runJsonAction(QString::fromUtf8("设置调节温度"),
                      [this, value](JsonCallback cb) {
            dev_->tempControl()->setAdjustTemperature(this, value, cb);
        });
    });
    connect(saveTargetButton_, &QPushButton::clicked, this, [this]() {
        runJsonAction(QString::fromUtf8("保存调节温度"),
                      [this](JsonCallback cb) {
            dev_->tempControl()->save(this, QStringLiteral("adjust_temperature"), cb);
        });
    });
    connect(switchOnButton_, &QPushButton::clicked, this, [this]() {
        runJsonAction(QString::fromUtf8("打开温控"),
                      [this](JsonCallback cb) { dev_->tempControl()->setSwitch(this, true, cb); });
    });
    connect(switchOffButton_, &QPushButton::clicked, this, [this]() {
        runJsonAction(QString::fromUtf8("关闭温控"),
                      [this](JsonCallback cb) { dev_->tempControl()->setSwitch(this, false, cb); });
    });

    connect(queryKeyButton_, &QPushButton::clicked, this, [this]() {
        const QString key = selectedKey();
        runJsonAction(QString::fromUtf8("查询%1").arg(selectedKeyName()),
                      [this, key](JsonCallback cb) { dev_->tempControl()->query(this, key, cb); });
    });
    connect(setKeyButton_, &QPushButton::clicked, this, [this]() {
        const QString key = selectedKey();
        const QString value = advancedValue();
        runJsonAction(QString::fromUtf8("设置%1").arg(selectedKeyName()),
                      [this, key, value](JsonCallback cb) {
            dev_->tempControl()->set(this, key, value, cb);
        });
    });
    connect(saveKeyButton_, &QPushButton::clicked, this, [this]() {
        const QString key = selectedKey();
        runJsonAction(QString::fromUtf8("保存%1").arg(selectedKeyName()),
                      [this, key](JsonCallback cb) { dev_->tempControl()->save(this, key, cb); });
    });

    connect(queryParamButton_, &QPushButton::clicked, this, [this]() {
        const QString module = moduleName();
        const QString param = paramName();
        runJsonAction(QString::fromUtf8("查询模块参数"),
                      [this, module, param](JsonCallback cb) {
            dev_->tempControl()->queryRawParam(this, module, param, cb);
        });
    });
    connect(setParamButton_, &QPushButton::clicked, this, [this]() {
        const QString module = moduleName();
        const QString param = paramName();
        const QString value = advancedValue();
        runJsonAction(QString::fromUtf8("设置模块参数"),
                      [this, module, param, value](JsonCallback cb) {
            dev_->tempControl()->setRawParam(this, module, param, value, cb);
        });
    });
    connect(saveParamButton_, &QPushButton::clicked, this, [this]() {
        const QString module = moduleName();
        const QString param = paramName();
        runJsonAction(QString::fromUtf8("保存模块参数"),
                      [this, module, param](JsonCallback cb) {
            dev_->tempControl()->saveRawParam(this, module, param, cb);
        });
    });
    connect(sendRawButton_, &QPushButton::clicked, this, [this]() {
        const QString command = rawCommandEdit_->text().trimmed();
        runJsonAction(QString::fromUtf8("发送原始命令"),
                      [this, command](JsonCallback cb) {
            dev_->tempControl()->sendRaw(this, command, cb);
        });
    });

    if (dev_ && dev_->isConnected()) {
        refreshStatus();
        refreshTimer_->start();
    } else {
        setStatusUnavailable(QString::fromUtf8("未连接"));
    }
}

void TempControlPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    auto* statusGroup = new QGroupBox(QString::fromUtf8("温控状态"), this);
    auto* statusForm = new QFormLayout(statusGroup);
    actualTemperatureLabel_ = makeReadout(this);
    adjustTemperatureLabel_ = makeReadout(this);
    switchLabel_ = makeReadout(this);
    outputEnabledLabel_ = makeReadout(this);
    errorStatusLabel_ = makeReadout(this);
    timestampLabel_ = makeReadout(this);
    statusForm->addRow(QString::fromUtf8("实际温度"), actualTemperatureLabel_);
    statusForm->addRow(QString::fromUtf8("调节温度"), adjustTemperatureLabel_);
    statusForm->addRow(QString::fromUtf8("温控开关"), switchLabel_);
    statusForm->addRow(QString::fromUtf8("功率输出"), outputEnabledLabel_);
    statusForm->addRow(QString::fromUtf8("错误状态"), errorStatusLabel_);
    statusForm->addRow(QString::fromUtf8("更新时间"), timestampLabel_);
    root->addWidget(statusGroup);

    auto* controlGroup = new QGroupBox(QString::fromUtf8("常用控制"), this);
    auto* controlForm = new QFormLayout(controlGroup);
    targetTemperatureSpin_ = new QDoubleSpinBox(this);
    targetTemperatureSpin_->setObjectName(QStringLiteral("tempControlTargetSpin"));
    targetTemperatureSpin_->setRange(-273.15, 1000.0);
    targetTemperatureSpin_->setDecimals(2);
    targetTemperatureSpin_->setSingleStep(0.1);
    targetTemperatureSpin_->setSuffix(QString::fromUtf8(" ℃"));
    setTargetButton_ = new QPushButton(QString::fromUtf8("设置"), this);
    setTargetButton_->setProperty("primary", true);
    saveTargetButton_ = new QPushButton(QString::fromUtf8("保存"), this);
    auto* targetRow = new QHBoxLayout();
    targetRow->addWidget(targetTemperatureSpin_, 1);
    targetRow->addWidget(setTargetButton_);
    targetRow->addWidget(saveTargetButton_);
    controlForm->addRow(QString::fromUtf8("目标温度"), targetRow);
    switchOnButton_ = new QPushButton(QString::fromUtf8("打开温控"), this);
    switchOnButton_->setProperty("primary", true);
    switchOffButton_ = new QPushButton(QString::fromUtf8("关闭温控"), this);
    switchOffButton_->setProperty("danger", true);
    addButtonRow(controlForm, {switchOnButton_, switchOffButton_});
    root->addWidget(controlGroup);

    auto* advancedGroup = new QGroupBox(QString::fromUtf8("高级参数"), this);
    auto* advancedForm = new QFormLayout(advancedGroup);
    keyCombo_ = new QComboBox(this);
    keyCombo_->setObjectName(QStringLiteral("tempControlKeyCombo"));
    keyCombo_->addItem(QString::fromUtf8("调节温度"), QStringLiteral("adjust_temperature"));
    keyCombo_->addItem(QString::fromUtf8("实际温度"), QStringLiteral("actual_temperature"));
    keyCombo_->addItem(QString::fromUtf8("设定温度"), QStringLiteral("set_temperature"));
    keyCombo_->addItem(QString::fromUtf8("温度限速"), QStringLiteral("ramp_speed"));
    keyCombo_->addItem(QString::fromUtf8("最高设定温度"), QStringLiteral("max_temperature"));
    keyCombo_->addItem(QString::fromUtf8("温度调节步进值"), QStringLiteral("step_temperature"));
    keyCombo_->addItem(QString::fromUtf8("最大输出电压"), QStringLiteral("max_voltage"));
    keyCombo_->addItem(QString::fromUtf8("冷热电压比值"), QStringLiteral("cool_voltage_ratio"));
    keyCombo_->addItem(QString::fromUtf8("输出模式"), QStringLiteral("mode"));
    keyCombo_->addItem(QString::fromUtf8("温控开关"), QStringLiteral("switch"));
    keyCombo_->addItem(QString::fromUtf8("功率输出状态"), QStringLiteral("output_enabled"));
    keyCombo_->addItem(QString::fromUtf8("预计输出电压"), QStringLiteral("set_voltage"));
    keyCombo_->addItem(QString::fromUtf8("实际输出电压"), QStringLiteral("actual_voltage"));
    keyCombo_->addItem(QString::fromUtf8("传感器类型"), QStringLiteral("sensor_type"));
    keyCombo_->addItem(QString::fromUtf8("传感器偏差"), QStringLiteral("temperature_offset"));
    keyCombo_->addItem(QString::fromUtf8("PID类型"), QStringLiteral("pid_type"));
    keyCombo_->addItem(QString::fromUtf8("PID比例系数"), QStringLiteral("pid_p"));
    keyCombo_->addItem(QString::fromUtf8("PID积分时间"), QStringLiteral("pid_ti"));
    keyCombo_->addItem(QString::fromUtf8("PID微分时间"), QStringLiteral("pid_td"));
    keyCombo_->addItem(QString::fromUtf8("PID控制间隔"), QStringLiteral("control_interval"));
    keyCombo_->addItem(QString::fromUtf8("PID公式"), QStringLiteral("pid_algorithm"));
    keyCombo_->addItem(QString::fromUtf8("自动整定进度"), QStringLiteral("tune_status"));
    keyCombo_->addItem(QString::fromUtf8("错误状态"), QStringLiteral("error_status"));
    keyCombo_->addItem(QString::fromUtf8("错误掩码"), QStringLiteral("error_mask"));
    advancedForm->addRow(QString::fromUtf8("参数"), keyCombo_);

    advancedValueEdit_ = new QLineEdit(this);
    advancedValueEdit_->setObjectName(QStringLiteral("tempControlAdvancedValueEdit"));
    advancedForm->addRow(QString::fromUtf8("设置值"), advancedValueEdit_);

    queryKeyButton_ = new QPushButton(QString::fromUtf8("查询参数"), this);
    setKeyButton_ = new QPushButton(QString::fromUtf8("设置参数"), this);
    saveKeyButton_ = new QPushButton(QString::fromUtf8("保存参数"), this);
    addButtonRow(advancedForm, {queryKeyButton_, setKeyButton_, saveKeyButton_});

    moduleEdit_ = new QLineEdit(this);
    moduleEdit_->setObjectName(QStringLiteral("tempControlModuleEdit"));
    moduleEdit_->setPlaceholderText(QStringLiteral("TC1"));
    paramEdit_ = new QLineEdit(this);
    paramEdit_->setObjectName(QStringLiteral("tempControlParamEdit"));
    paramEdit_->setPlaceholderText(QStringLiteral("TCACTTEMP"));
    advancedForm->addRow(QString::fromUtf8("模块"), moduleEdit_);
    advancedForm->addRow(QString::fromUtf8("协议参数"), paramEdit_);

    queryParamButton_ = new QPushButton(QString::fromUtf8("查询模块参数"), this);
    setParamButton_ = new QPushButton(QString::fromUtf8("设置模块参数"), this);
    saveParamButton_ = new QPushButton(QString::fromUtf8("保存模块参数"), this);
    addButtonRow(advancedForm, {queryParamButton_, setParamButton_, saveParamButton_});

    rawCommandEdit_ = new QLineEdit(this);
    rawCommandEdit_->setObjectName(QStringLiteral("tempControlRawCommandEdit"));
    rawCommandEdit_->setPlaceholderText(QStringLiteral("TC1:TCACTTEMP?"));
    sendRawButton_ = new QPushButton(QString::fromUtf8("发送原始命令"), this);
    auto* rawRow = new QHBoxLayout();
    rawRow->addWidget(rawCommandEdit_, 1);
    rawRow->addWidget(sendRawButton_);
    advancedForm->addRow(QString::fromUtf8("Raw"), rawRow);

    resultLabel_ = makeReadout(this);
    resultLabel_->setWordWrap(true);
    advancedForm->addRow(QString::fromUtf8("结果"), resultLabel_);
    root->addWidget(advancedGroup);
    root->addStretch();
}

void TempControlPanel::loadSettings()
{
    QSettings s;
    targetTemperatureSpin_->setValue(s.value(kSettingsPrefix + QStringLiteral("targetTemperature"),
                                             targetTemperatureSpin_->value()).toDouble());
    const QString selectedKeyValue = s.value(kSettingsPrefix + QStringLiteral("selectedKey"),
                                             QStringLiteral("adjust_temperature")).toString();
    const int keyIndex = keyCombo_->findData(selectedKeyValue);
    if (keyIndex >= 0) {
        keyCombo_->setCurrentIndex(keyIndex);
    }
    advancedValueEdit_->setText(s.value(kSettingsPrefix + QStringLiteral("advancedValue")).toString());
    moduleEdit_->setText(s.value(kSettingsPrefix + QStringLiteral("module"),
                                 QStringLiteral("TC1")).toString());
    paramEdit_->setText(s.value(kSettingsPrefix + QStringLiteral("param")).toString());
    rawCommandEdit_->setText(s.value(kSettingsPrefix + QStringLiteral("rawCommand")).toString());
}

void TempControlPanel::saveSettings() const
{
    QSettings s;
    s.setValue(kSettingsPrefix + QStringLiteral("targetTemperature"), targetTemperatureSpin_->value());
    s.setValue(kSettingsPrefix + QStringLiteral("selectedKey"), selectedKey());
    s.setValue(kSettingsPrefix + QStringLiteral("advancedValue"), advancedValueEdit_->text());
    s.setValue(kSettingsPrefix + QStringLiteral("module"), moduleEdit_->text());
    s.setValue(kSettingsPrefix + QStringLiteral("param"), paramEdit_->text());
    s.setValue(kSettingsPrefix + QStringLiteral("rawCommand"), rawCommandEdit_->text());
}

void TempControlPanel::refreshStatus()
{
    if (!dev_ || !dev_->isConnected()) {
        setStatusUnavailable(QString::fromUtf8("未连接"));
        return;
    }
    if (statusPending_) {
        return;
    }
    statusPending_ = true;
    dev_->tempControl()->status(this, [this](bool ok, const TempControlStatus& status, const QString& err) {
        statusPending_ = false;
        if (ok) {
            updateStatusUi(status);
        } else {
            setStatusUnavailable(err.isEmpty() ? QString::fromUtf8("状态读取失败") : err);
        }
    });
}

void TempControlPanel::setStatusUnavailable(const QString& reason)
{
    statusPending_ = false;
    actualTemperatureLabel_->setText(QStringLiteral("-"));
    adjustTemperatureLabel_->setText(QStringLiteral("-"));
    switchLabel_->setText(reason);
    outputEnabledLabel_->setText(QStringLiteral("-"));
    errorStatusLabel_->setText(QStringLiteral("-"));
    timestampLabel_->setText(QStringLiteral("-"));
}

void TempControlPanel::updateStatusUi(const TempControlStatus& status)
{
    actualTemperatureLabel_->setText(QString::number(status.actualTemperature, 'f', 2) + QString::fromUtf8(" ℃"));
    adjustTemperatureLabel_->setText(QString::number(status.adjustTemperature, 'f', 2) + QString::fromUtf8(" ℃"));
    switchLabel_->setText(boolStateText(status.switchEnabled));
    outputEnabledLabel_->setText(boolStateText(status.outputEnabled));
    errorStatusLabel_->setText(status.errorStatus.isEmpty() ? QStringLiteral("-") : status.errorStatus);
    if (status.timestamp > 0) {
        timestampLabel_->setText(QDateTime::fromMSecsSinceEpoch(status.timestamp).toString(Qt::ISODate));
    } else {
        timestampLabel_->setText(QDateTime::currentDateTime().toString(Qt::ISODate));
    }
}

void TempControlPanel::setResult(const QString& text)
{
    resultLabel_->setText(text);
}

void TempControlPanel::runJsonAction(const QString& label, const std::function<void(JsonCallback)>& action)
{
    if (!dev_ || !dev_->isConnected()) {
        setResult(QString::fromUtf8("%1失败：未连接").arg(label));
        return;
    }
    setResult(QString::fromUtf8("%1中...").arg(label));
    action([this, label](bool ok, const nlohmann::json& data, const QString& err) {
        if (ok) {
            setResult(QString::fromUtf8("%1成功：%2").arg(label, jsonToText(data)));
            refreshStatus();
        } else {
            setResult(QString::fromUtf8("%1失败：%2").arg(label, err));
        }
    });
}

QString TempControlPanel::selectedKey() const
{
    return keyCombo_->currentData().toString();
}

QString TempControlPanel::selectedKeyName() const
{
    return keyCombo_->currentText();
}

QString TempControlPanel::advancedValue() const
{
    return advancedValueEdit_->text().trimmed();
}

QString TempControlPanel::moduleName() const
{
    return moduleEdit_->text().trimmed();
}

QString TempControlPanel::paramName() const
{
    return paramEdit_->text().trimmed();
}

QString TempControlPanel::jsonToText(const nlohmann::json& data)
{
    if (data.is_null()) {
        return QString::fromUtf8("无返回数据");
    }
    if (data.is_object()) {
        const auto typedValue = data.find("typed_value");
        if (typedValue != data.end()) {
            return QString::fromStdString(typedValue->dump());
        }
        const auto value = data.find("value");
        if (value != data.end()) {
            return QString::fromStdString(value->dump());
        }
        const auto raw = data.find("raw");
        if (raw != data.end() && raw->is_string()) {
            return QString::fromStdString(raw->get<std::string>());
        }
    }
    return QString::fromStdString(data.dump());
}

#include "CollectPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>

CollectPanel::CollectPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* grp = new QGroupBox(QString::fromUtf8("运动采集门控"), this);
    auto* fl = new QFormLayout(grp);

    statusLabel_ = new QLabel(QString::fromUtf8("未知"), this);
    statusLabel_->setProperty("readout", true);
    fl->addRow(QString::fromUtf8("状态"), statusLabel_);

    oversampleFactorSpin_ = new QSpinBox(this);
    oversampleFactorSpin_->setObjectName(QStringLiteral("collectOversampleFactorSpin"));
    oversampleFactorSpin_->setRange(1, 1024);
    oversampleFactorSpin_->setValue(1);
    applyOversamplingBtn_ = new QPushButton(QString::fromUtf8("应用"), this);
    auto* oversamplingRow = new QHBoxLayout();
    oversamplingRow->addWidget(oversampleFactorSpin_, 1);
    oversamplingRow->addWidget(applyOversamplingBtn_);
    fl->addRow(QString::fromUtf8("超采样倍率"), oversamplingRow);

    effectiveSSpeedLabel_ = new QLabel(QString::fromUtf8("-"), this);
    effectiveSSpeedLabel_->setProperty("readout", true);
    effectiveFSpeedLabel_ = new QLabel(QString::fromUtf8("-"), this);
    effectiveFSpeedLabel_->setProperty("readout", true);
    fl->addRow(QString::fromUtf8("有效 S 速度"), effectiveSSpeedLabel_);
    fl->addRow(QString::fromUtf8("有效 F 速度"), effectiveFSpeedLabel_);

    discardFrontMsSpin_ = new QSpinBox(this);
    discardFrontMsSpin_->setObjectName(QStringLiteral("collectDiscardFrontMsSpin"));
    discardFrontMsSpin_->setRange(0, 600000);
    discardFrontMsSpin_->setSuffix(QStringLiteral(" ms"));
    discardBackMsSpin_ = new QSpinBox(this);
    discardBackMsSpin_->setObjectName(QStringLiteral("collectDiscardBackMsSpin"));
    discardBackMsSpin_->setRange(0, 600000);
    discardBackMsSpin_->setSuffix(QStringLiteral(" ms"));
    forwardOffsetFramesSpin_ = new QSpinBox(this);
    forwardOffsetFramesSpin_->setObjectName(QStringLiteral("collectForwardOffsetFramesSpin"));
    forwardOffsetFramesSpin_->setRange(-100000, 100000);
    reverseOffsetFramesSpin_ = new QSpinBox(this);
    reverseOffsetFramesSpin_->setObjectName(QStringLiteral("collectReverseOffsetFramesSpin"));
    reverseOffsetFramesSpin_->setRange(-100000, 100000);

    gateCollectingLabel_ = new QLabel(QString::fromUtf8("未知"), this);
    gateCollectingLabel_->setProperty("readout", true);
    gatePendingLabel_ = new QLabel(QString::fromUtf8("未知"), this);
    gatePendingLabel_->setProperty("readout", true);
    fl->addRow(QString::fromUtf8("前段丢弃时间"), discardFrontMsSpin_);
    fl->addRow(QString::fromUtf8("后段过扫时间"), discardBackMsSpin_);
    fl->addRow(QString::fromUtf8("正向补偿帧"), forwardOffsetFramesSpin_);
    fl->addRow(QString::fromUtf8("反向补偿帧"), reverseOffsetFramesSpin_);
    fl->addRow(QString::fromUtf8("门控采集状态"), gateCollectingLabel_);
    fl->addRow(QString::fromUtf8("门控配置状态"), gatePendingLabel_);

    auto* gateRow = new QHBoxLayout();
    refreshGateConfigBtn_ = new QPushButton(QString::fromUtf8("读取门控"), this);
    applyGateConfigBtn_ = new QPushButton(QString::fromUtf8("应用门控"), this);
    gateRow->addWidget(refreshGateConfigBtn_);
    gateRow->addWidget(applyGateConfigBtn_);
    fl->addRow(gateRow);

    auto* row = new QHBoxLayout();
    startBtn_ = new QPushButton(QString::fromUtf8("开始"), this);
    startBtn_->setProperty("primary", true);
    stopBtn_ = new QPushButton(QString::fromUtf8("停止"), this);
    stopBtn_->setProperty("danger", true);
    refreshBtn_ = new QPushButton(QString::fromUtf8("刷新"), this);
    row->addWidget(startBtn_);
    row->addWidget(stopBtn_);
    row->addWidget(refreshBtn_);
    fl->addRow(row);

    root->addWidget(grp);
    root->addStretch();

    loadSettings();

    auto cb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "Collect", err);
        else refreshStatus();
    };

    connect(startBtn_, &QPushButton::clicked, this, [this, cb]() { dev_->collect()->start(this, cb); });
    connect(stopBtn_, &QPushButton::clicked, this, [this, cb]() { dev_->collect()->stop(this, cb); });
    connect(refreshBtn_, &QPushButton::clicked, this, &CollectPanel::refreshStatus);
    connect(applyOversamplingBtn_, &QPushButton::clicked, this, &CollectPanel::applyOversampling);
    connect(refreshGateConfigBtn_, &QPushButton::clicked, this, &CollectPanel::refreshGateConfig);
    connect(applyGateConfigBtn_, &QPushButton::clicked, this, &CollectPanel::applyGateConfig);
    connect(oversampleFactorSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    connect(discardFrontMsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    connect(discardBackMsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    connect(forwardOffsetFramesSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });
    connect(reverseOffsetFramesSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { saveSettings(); });

    connect(dev, &DeviceClient::connectionChanged, this, [this](bool c, const QString&) {
        if (c) refreshStatus();
    });
}

void CollectPanel::loadSettings()
{
    QSettings s;
    const QString p = QStringLiteral("panels/collect/");
    oversampleFactorSpin_->setValue(s.value(p + QStringLiteral("oversampleFactor"), oversampleFactorSpin_->value()).toInt());
    discardFrontMsSpin_->setValue(s.value(p + QStringLiteral("discardFrontMs"), discardFrontMsSpin_->value()).toInt());
    discardBackMsSpin_->setValue(s.value(p + QStringLiteral("discardBackMs"), discardBackMsSpin_->value()).toInt());
    forwardOffsetFramesSpin_->setValue(s.value(p + QStringLiteral("forwardOffsetFrames"), forwardOffsetFramesSpin_->value()).toInt());
    reverseOffsetFramesSpin_->setValue(s.value(p + QStringLiteral("reverseOffsetFrames"), reverseOffsetFramesSpin_->value()).toInt());
}

void CollectPanel::saveSettings() const
{
    QSettings s;
    const QString p = QStringLiteral("panels/collect/");
    s.setValue(p + QStringLiteral("oversampleFactor"), oversampleFactorSpin_->value());
    s.setValue(p + QStringLiteral("discardFrontMs"), discardFrontMsSpin_->value());
    s.setValue(p + QStringLiteral("discardBackMs"), discardBackMsSpin_->value());
    s.setValue(p + QStringLiteral("forwardOffsetFrames"), forwardOffsetFramesSpin_->value());
    s.setValue(p + QStringLiteral("reverseOffsetFrames"), reverseOffsetFramesSpin_->value());
}

void CollectPanel::refreshStatus()
{
    dev_->collect()->getOversampling(this, [this](bool ok, const CollectOversamplingInfo& info, const QString&) {
        if (ok) {
            updateOversamplingUi(info);
        } else {
            statusLabel_->setText(QString::fromUtf8("未知"));
            effectiveSSpeedLabel_->setText(QString::fromUtf8("-"));
            effectiveFSpeedLabel_->setText(QString::fromUtf8("-"));
        }
    });
    refreshGateConfig();
}

void CollectPanel::applyOversampling()
{
    const int factor = oversampleFactorSpin_->value();
    applyOversamplingBtn_->setEnabled(false);
    dev_->collect()->setOversampling(this, factor,
        [this](bool ok, const CollectOversamplingInfo& info, const QString& err) {
            if (ok) {
                updateOversamplingUi(info);
            } else {
                QMessageBox::warning(this, "Collect", err);
                applyOversamplingBtn_->setEnabled(true);
                refreshStatus();
            }
        });
}

void CollectPanel::refreshGateConfig()
{
    refreshGateConfigBtn_->setEnabled(false);
    dev_->collect()->getGateConfig(this, [this](bool ok, const CollectGateConfig& config, const QString& err) {
        refreshGateConfigBtn_->setEnabled(true);
        if (ok) {
            updateGateConfigUi(config);
        } else {
            gateCollectingLabel_->setText(QString::fromUtf8("未知"));
            gatePendingLabel_->setText(err.isEmpty() ? QString::fromUtf8("读取失败") : err);
        }
    });
}

void CollectPanel::applyGateConfig()
{
    CollectGateConfig config;
    config.discardFrontMs = discardFrontMsSpin_->value();
    config.discardBackMs = discardBackMsSpin_->value();
    config.forwardOffsetFrames = forwardOffsetFramesSpin_->value();
    config.reverseOffsetFrames = reverseOffsetFramesSpin_->value();

    applyGateConfigBtn_->setEnabled(false);
    dev_->collect()->setGateConfig(this, config,
        [this](bool ok, const CollectGateConfig& updatedConfig, const QString& err) {
            applyGateConfigBtn_->setEnabled(true);
            if (ok) {
                updateGateConfigUi(updatedConfig);
            } else {
                QMessageBox::warning(this, "Collect", err);
            }
        });
}

void CollectPanel::updateOversamplingUi(const CollectOversamplingInfo& info)
{
    statusLabel_->setText(info.collecting
        ? QString::fromUtf8("采集中")
        : QString::fromUtf8("已停止"));
    {
        const QSignalBlocker blocker(oversampleFactorSpin_);
        oversampleFactorSpin_->setValue(info.oversampleFactor);
    }
    effectiveSSpeedLabel_->setText(QString::number(info.effectiveSSpeed));
    effectiveFSpeedLabel_->setText(QString::number(info.effectiveFSpeed));
    oversampleFactorSpin_->setEnabled(!info.collecting);
    applyOversamplingBtn_->setEnabled(!info.collecting);
}

void CollectPanel::updateGateConfigUi(const CollectGateConfig& config)
{
    {
        const QSignalBlocker blocker(discardFrontMsSpin_);
        discardFrontMsSpin_->setValue(config.discardFrontMs);
    }
    {
        const QSignalBlocker blocker(discardBackMsSpin_);
        discardBackMsSpin_->setValue(config.discardBackMs);
    }
    {
        const QSignalBlocker blocker(forwardOffsetFramesSpin_);
        forwardOffsetFramesSpin_->setValue(config.forwardOffsetFrames);
    }
    {
        const QSignalBlocker blocker(reverseOffsetFramesSpin_);
        reverseOffsetFramesSpin_->setValue(config.reverseOffsetFrames);
    }

    gateCollectingLabel_->setText(config.collecting
        ? QString::fromUtf8("采集中")
        : QString::fromUtf8("已停止"));
    gatePendingLabel_->setText(config.pendingConfig
        ? QString::fromUtf8("下一段生效")
        : QString::fromUtf8("已生效"));
    applyGateConfigBtn_->setEnabled(true);
}

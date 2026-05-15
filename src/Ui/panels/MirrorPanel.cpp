#include "MirrorPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>

MirrorPanel::MirrorPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto simpleCb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "Mirror", err);
    };

    // realtime angle
    auto* stGrp = new QGroupBox(QString::fromUtf8("当前状态"), this);
    auto* stForm = new QFormLayout(stGrp);
    angleLabel_ = new QLabel("-", this);
    angleLabel_->setProperty("readout", true);
    movingLabel_ = new QLabel("-", this);
    stForm->addRow(QString::fromUtf8("角度"), angleLabel_);
    stForm->addRow(QString::fromUtf8("运动"), movingLabel_);
    root->addWidget(stGrp);

    // target
    auto* tgGrp = new QGroupBox(QString::fromUtf8("目标角度"), this);
    auto* tgForm = new QFormLayout(tgGrp);
    targetSpin_ = new QDoubleSpinBox(this);
    targetSpin_->setRange(-9999.0, 9999.0);
    targetSpin_->setDecimals(3);
    tgForm->addRow(QString::fromUtf8("角度"), targetSpin_);

    auto* tgRow = new QHBoxLayout();
    setTargetBtn_ = new QPushButton(QString::fromUtf8("相对"), this);
    setAbsBtn_    = new QPushButton(QString::fromUtf8("绝对"), this);
    startBtn_     = new QPushButton(QString::fromUtf8("开始"), this);
    startBtn_->setProperty("primary", true);
    stopBtn_      = new QPushButton(QString::fromUtf8("停止"), this);
    stopBtn_->setProperty("danger", true);
    tgRow->addWidget(setTargetBtn_);
    tgRow->addWidget(setAbsBtn_);
    tgRow->addWidget(startBtn_);
    tgRow->addWidget(stopBtn_);
    tgForm->addRow(tgRow);
    root->addWidget(tgGrp);

    // speed
    auto* spGrp = new QGroupBox(QString::fromUtf8("速度"), this);
    auto* spForm = new QFormLayout(spGrp);
    sSpeedSpin_ = new QSpinBox(this); sSpeedSpin_->setRange(1, 50000); sSpeedSpin_->setValue(400);
    fSpeedSpin_ = new QSpinBox(this); fSpeedSpin_->setRange(1, 50000); fSpeedSpin_->setValue(400);
    applySpeedBtn_ = new QPushButton(QString::fromUtf8("应用"), this);
    spForm->addRow("S Speed", sSpeedSpin_);
    spForm->addRow("F Speed", fSpeedSpin_);
    spForm->addRow(applySpeedBtn_);
    root->addWidget(spGrp);

    // home
    auto* homeRow = new QHBoxLayout();
    homeBtn_    = new QPushButton("Home", this);
    setHomeBtn_ = new QPushButton(QString::fromUtf8("设为原点"), this);
    homeRow->addWidget(homeBtn_);
    homeRow->addWidget(setHomeBtn_);
    root->addLayout(homeRow);

    // preset
    auto* preRow = new QHBoxLayout();
    presetCombo_ = new QComboBox(this);
    for (int i = 0; i < 10; ++i)
        presetCombo_->addItem(QString("Preset %1").arg(i), i);
    gotoPresetBtn_ = new QPushButton("Go", this);
    gotoPresetBtn_->setProperty("primary", true);
    preRow->addWidget(presetCombo_, 1);
    preRow->addWidget(gotoPresetBtn_);
    root->addLayout(preRow);

    // query
    queryBtn_ = new QPushButton(QString::fromUtf8("查询角度"), this);
    root->addWidget(queryBtn_);
    root->addStretch();

    // connections
    connect(setTargetBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorSetTarget(targetSpin_->value(), simpleCb);
    });
    connect(setAbsBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorSetTargetAbsolute(targetSpin_->value(), simpleCb);
    });
    connect(startBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorStartMove(simpleCb);
    });
    connect(stopBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorStopMove(simpleCb);
    });
    connect(applySpeedBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorSetSpeed(sSpeedSpin_->value(), fSpeedSpin_->value(), simpleCb);
    });
    connect(homeBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorHome(simpleCb);
    });
    connect(setHomeBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        if (QMessageBox::question(this, "Mirror",
            QString::fromUtf8("确认设置当前位置为原点？")) == QMessageBox::Yes)
            dev_->mirrorSetHome(simpleCb);
    });
    connect(gotoPresetBtn_, &QPushButton::clicked, this, [this, simpleCb]() {
        dev_->mirrorGotoPreset(presetCombo_->currentData().toInt(), simpleCb);
    });
    connect(queryBtn_, &QPushButton::clicked, this, &MirrorPanel::onQueryAngle);

    connect(dev, &DeviceClient::mirrorAngleEvent, this,
            [this](double angle, bool moving, qint64) {
        angleLabel_->setText(QString::number(angle, 'f', 3) + QString::fromUtf8("°"));
        movingLabel_->setText(moving ? QString::fromUtf8("运动中") : QString::fromUtf8("已停止"));
        movingLabel_->setStyleSheet(moving ? "color: #E9B949;" : "color: #3FB950;");
    });
}

void MirrorPanel::onQueryAngle()
{
    dev_->mirrorQueryAngle([this](bool ok, double angle, bool moving, const QString& err) {
        if (ok) {
            angleLabel_->setText(QString::number(angle, 'f', 3) + QString::fromUtf8("°"));
            movingLabel_->setText(moving ? QString::fromUtf8("运动中") : QString::fromUtf8("已停止"));
        } else {
            QMessageBox::warning(this, "Mirror", err);
        }
    });
}

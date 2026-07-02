#include "CalibrationPanel.h"

#include "DeviceClient.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>

CalibrationPanel::CalibrationPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent)
    , dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* backgroundCalibrationGroup = new QGroupBox(QString::fromUtf8("背景校正"), this);
    auto* backgroundCalibrationForm = new QFormLayout(backgroundCalibrationGroup);
    backgroundCalibrationStatusLabel_ = new QLabel(QString::fromUtf8("未启动"), this);
    backgroundCalibrationStatusLabel_->setObjectName(QStringLiteral("calibrationBackgroundCalibrationStatusLabel"));
    backgroundCalibrationStatusLabel_->setProperty("readout", true);
    backgroundCalibrationBtn_ = new QPushButton(QString::fromUtf8("开始背景校正"), this);
    backgroundCalibrationBtn_->setObjectName(QStringLiteral("calibrationBackgroundCalibrationButton"));
    backgroundCalibrationBtn_->setProperty("primary", true);
    backgroundCalibrationForm->addRow(QString::fromUtf8("状态"), backgroundCalibrationStatusLabel_);
    backgroundCalibrationForm->addRow(backgroundCalibrationBtn_);
    root->addWidget(backgroundCalibrationGroup);
    root->addStretch();

    connect(backgroundCalibrationBtn_, &QPushButton::clicked,
            this, &CalibrationPanel::startBackgroundCalibration);

    backgroundCalibrationTimer_ = new QTimer(this);
    backgroundCalibrationTimer_->setSingleShot(true);
    connect(backgroundCalibrationTimer_, &QTimer::timeout,
            this, &CalibrationPanel::pollBackgroundCalibrationStatus);
}

void CalibrationPanel::startBackgroundCalibration()
{
    const auto answer = QMessageBox::question(
        this, QString::fromUtf8("背景校正"),
        QString::fromUtf8("背景校正会移动转镜并触发红外机芯校正，是否继续？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    backgroundCalibrationBtn_->setEnabled(false);
    dev_->systemApi()->startBackgroundCalibration(
        this, [this](bool ok, const BackgroundCalibrationStart& result, const QString& err) {
            if (!ok) {
                backgroundCalibrationBtn_->setEnabled(true);
                QMessageBox::warning(this, QString::fromUtf8("背景校正"), err);
                return;
            }
            setBackgroundCalibrationStage(result.stage);
            backgroundCalibrationTimer_->start(300);
        });
}

void CalibrationPanel::pollBackgroundCalibrationStatus()
{
    dev_->systemApi()->backgroundCalibrationStatus(
        this, [this](bool ok, const BackgroundCalibrationStatus& status, const QString& err) {
            if (!ok) {
                backgroundCalibrationBtn_->setEnabled(true);
                backgroundCalibrationStatusLabel_->setText(
                    err.isEmpty() ? QString::fromUtf8("状态查询失败") : err);
                QMessageBox::warning(this, QString::fromUtf8("背景校正"), err);
                return;
            }

            setBackgroundCalibrationStage(status.stage, status.error);
            if (status.running) {
                backgroundCalibrationTimer_->start(300);
            } else {
                finishBackgroundCalibration(status);
            }
        });
}

void CalibrationPanel::finishBackgroundCalibration(const BackgroundCalibrationStatus& status)
{
    backgroundCalibrationTimer_->stop();
    backgroundCalibrationBtn_->setEnabled(true);
    if (status.stage == QStringLiteral("failed")) {
        QMessageBox::warning(this, QString::fromUtf8("背景校正"),
                             status.error.isEmpty() ? QString::fromUtf8("背景校正失败") : status.error);
    }
}

void CalibrationPanel::setBackgroundCalibrationStage(const QString& stage, const QString& error)
{
    QString text;
    if (stage == QStringLiteral("idle")) text = QString::fromUtf8("未启动");
    else if (stage == QStringLiteral("moving_to_background")) text = QString::fromUtf8("正在前往背景位");
    else if (stage == QStringLiteral("calibrating")) text = QString::fromUtf8("正在背景校正");
    else if (stage == QStringLiteral("restoring")) text = QString::fromUtf8("正在复位");
    else if (stage == QStringLiteral("completed")) text = QString::fromUtf8("背景校正完成");
    else if (stage == QStringLiteral("failed")) text = QString::fromUtf8("背景校正失败");
    else text = stage;

    if (!error.isEmpty()) text += QStringLiteral(": ") + error;
    backgroundCalibrationStatusLabel_->setText(text);
}

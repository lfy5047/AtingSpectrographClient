#include "CameraPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>

CameraPanel::CameraPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto simpleCb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "Camera", err);
    };

    // resolution
    auto* resGrp = new QGroupBox(QString::fromUtf8("分辨率"), this);
    auto* resForm = new QFormLayout(resGrp);
    curResLabel_ = new QLabel("-", this);
    curResLabel_->setProperty("readout", true);
    resForm->addRow(QString::fromUtf8("当前"), curResLabel_);

    widthSpin_ = new QSpinBox(this);
    widthSpin_->setRange(1, 4096); widthSpin_->setSingleStep(8); widthSpin_->setValue(1280);
    heightSpin_ = new QSpinBox(this);
    heightSpin_->setRange(1, 4096); heightSpin_->setSingleStep(8); heightSpin_->setValue(1024);

    resForm->addRow(QString::fromUtf8("宽"), widthSpin_);
    resForm->addRow(QString::fromUtf8("高"), heightSpin_);

    auto* resRow = new QHBoxLayout();
    applyBtn_ = new QPushButton(QString::fromUtf8("应用"), this);
    applyBtn_->setProperty("primary", true);
    refreshBtn_ = new QPushButton(QString::fromUtf8("刷新"), this);
    resRow->addWidget(applyBtn_);
    resRow->addWidget(refreshBtn_);
    resForm->addRow(resRow);
    root->addWidget(resGrp);

    // stream control
    auto* ctrlGrp = new QGroupBox(QString::fromUtf8("采集控制"), this);
    auto* ctrlRow = new QHBoxLayout(ctrlGrp);
    startBtn_ = new QPushButton(QString::fromUtf8("开始采集"), this);
    startBtn_->setProperty("primary", true);
    stopBtn_ = new QPushButton(QString::fromUtf8("停止采集"), this);
    stopBtn_->setProperty("danger", true);
    ctrlRow->addWidget(startBtn_);
    ctrlRow->addWidget(stopBtn_);
    root->addWidget(ctrlGrp);

    root->addStretch();

    connect(applyBtn_, &QPushButton::clicked, this, &CameraPanel::onApplyResolution);
    connect(refreshBtn_, &QPushButton::clicked, this, &CameraPanel::refreshResolution);
    connect(startBtn_, &QPushButton::clicked, this, [this, simpleCb]() { dev_->cameraStartStream(simpleCb); });
    connect(stopBtn_, &QPushButton::clicked, this, [this, simpleCb]() { dev_->cameraStopStream(simpleCb); });

    connect(dev, &DeviceClient::connectionChanged, this, [this](bool c, const QString&) {
        if (c) refreshResolution();
    });
}

void CameraPanel::refreshResolution()
{
    dev_->cameraGetResolution([this](bool ok, int w, int h, const QString&) {
        if (ok) {
            curResLabel_->setText(QString("%1 x %2").arg(w).arg(h));
            widthSpin_->setValue(w);
            heightSpin_->setValue(h);
        }
    });
}

void CameraPanel::onApplyResolution()
{
    int w = widthSpin_->value(), h = heightSpin_->value();
    if (QMessageBox::question(this, "Camera",
        QString::fromUtf8("确认修改分辨率为 %1x%2？").arg(w).arg(h)) == QMessageBox::Yes)
    {
        dev_->cameraSetResolution(w, h, [this](bool ok, const QString& err) {
            if (ok) refreshResolution();
            else QMessageBox::warning(this, "Camera", err);
        });
    }
}

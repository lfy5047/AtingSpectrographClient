#include "CameraPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QSizePolicy>

CameraPanel::CameraPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto simpleCb = [this](bool ok, const QString& err) {
        if (!ok) QMessageBox::warning(this, "Camera", err);
    };

    // device select
    auto* devGrp = new QGroupBox(QString::fromUtf8("采集设备"), this);
    auto* devForm = new QFormLayout(devGrp);

    auto* devRow = new QHBoxLayout();
    deviceCombo_ = new QComboBox(this);
    deviceCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    deviceRefreshBtn_ = new QPushButton(QString::fromUtf8("刷新"), this);
    devRow->addWidget(deviceCombo_, 1);
    devRow->addWidget(deviceRefreshBtn_);
    devForm->addRow(QString::fromUtf8("选择"), devRow);
    root->addWidget(devGrp);

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
    connect(startBtn_, &QPushButton::clicked, this, [this, simpleCb]() { dev_->camera()->startStream(this, simpleCb); });
    connect(stopBtn_, &QPushButton::clicked, this, [this, simpleCb]() { dev_->camera()->stopStream(this, simpleCb); });
    connect(deviceRefreshBtn_, &QPushButton::clicked, this, &CameraPanel::refreshDevices);
    connect(deviceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraPanel::onDeviceChanged);

    connect(dev, &DeviceClient::connectionChanged, this, [this](bool c, const QString&) {
        if (c) {
            refreshResolution();
            refreshDevices();
        } else {
            deviceSelectedMac_.clear();
            reloadDeviceUi();
        }
    });

    reloadDeviceUi();
}

void CameraPanel::refreshResolution()
{
    dev_->camera()->getResolution(this, [this](bool ok, int w, int h, const QString&) {
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
        dev_->camera()->setResolution(this, w, h, [this](bool ok, const QString& err) {
            if (ok) refreshResolution();
            else QMessageBox::warning(this, "Camera", err);
        });
    }
}

void CameraPanel::refreshDevices()
{
    deviceRefreshing_ = true;
    deviceCombo_->setEnabled(false);
    deviceRefreshBtn_->setEnabled(false);

    dev_->camera()->deviceOptions(this,
        [this](bool ok, const std::vector<CameraDeviceOption>& options, const QString& err) {
            if (!ok && dev_->isConnected())
                QMessageBox::warning(this, "Camera", err);

            deviceRefreshing_ = true;
            deviceCombo_->clear();
            deviceCombo_->addItem(QString::fromUtf8("默认"), QString());
            for (const auto& opt : options) {
                QString text = opt.mac;
                if (!opt.name.isEmpty()) text = QString("%1").arg(opt.name);
                deviceCombo_->addItem(text, opt.mac);
            }
            deviceRefreshing_ = false;

            dev_->camera()->getSelectedDevice(this,
                [this](bool ok2, const QString& mac, const QString& err2) {
                    if (ok2) {
                        deviceSelectedMac_ = mac;
                        applySelectedMac(mac);
                    } else {
                        if (dev_->isConnected())
                            QMessageBox::warning(this, "Camera", err2);
                        deviceSelectedMac_.clear();
                        applySelectedMac(QString());
                    }

                    bool en = dev_->isConnected();
                    deviceCombo_->setEnabled(en);
                    deviceRefreshBtn_->setEnabled(en);
                });
        });
}

void CameraPanel::onDeviceChanged(int index)
{
    if (deviceRefreshing_) return;
    if (!dev_->isConnected()) return;
    if (index < 0) return;

    const QString mac = deviceCombo_->itemData(index).toString();

    deviceRefreshing_ = true;
    deviceCombo_->setEnabled(false);
    deviceRefreshBtn_->setEnabled(false);

    if (mac.isEmpty()) {
        dev_->camera()->clearSelectedDevice(this, [this](bool ok, const QString& err) {
            if (!ok && dev_->isConnected()) QMessageBox::warning(this, "Camera", err);
            refreshDevices();
        });
    } else {
        dev_->camera()->selectDevice(this, mac, [this](bool ok, const QString& err) {
            if (!ok && dev_->isConnected()) QMessageBox::warning(this, "Camera", err);
            refreshDevices();
        });
    }
}

void CameraPanel::reloadDeviceUi()
{
    deviceRefreshing_ = true;
    deviceCombo_->clear();
    deviceCombo_->addItem(QString::fromUtf8("默认"), QString());
    deviceCombo_->setCurrentIndex(0);
    deviceRefreshing_ = false;

    bool en = dev_->isConnected();
    deviceCombo_->setEnabled(en);
    deviceRefreshBtn_->setEnabled(en);
}

void CameraPanel::applySelectedMac(const QString& mac)
{
    deviceRefreshing_ = true;
    int idx = 0;
    if (!mac.isEmpty()) {
        for (int i = 1; i < deviceCombo_->count(); ++i) {
            if (deviceCombo_->itemData(i).toString() == mac) {
                idx = i;
                break;
            }
        }
    }
    deviceCombo_->setCurrentIndex(idx);
    deviceRefreshing_ = false;
}

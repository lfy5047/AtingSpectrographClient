#include "AdvancedSettingsPanel.h"

#include "CameraPanel.h"
#include "CollectPanel.h"
#include "ConnectionPanel.h"

#include <QGroupBox>
#include <QVBoxLayout>

AdvancedSettingsPanel::AdvancedSettingsPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    auto* connectionGroup = new QGroupBox(QString::fromUtf8("连接参数"), this);
    auto* connectionLayout = new QVBoxLayout(connectionGroup);
    connectionPanel_ = new ConnectionPanel(dev, ConnectionPanel::SettingsWithToggle, connectionGroup);
    connectionPanel_->setObjectName(QStringLiteral("advancedConnectionPanel"));
    connectionLayout->addWidget(connectionPanel_);
    root->addWidget(connectionGroup);

    auto* cameraGroup = new QGroupBox(QString::fromUtf8("相机设置"), this);
    auto* cameraLayout = new QVBoxLayout(cameraGroup);
    cameraPanel_ = new CameraPanel(dev, cameraGroup);
    cameraPanel_->setObjectName(QStringLiteral("advancedCameraPanel"));
    cameraLayout->addWidget(cameraPanel_);
    root->addWidget(cameraGroup);

    auto* collectGroup = new QGroupBox(QString::fromUtf8("采集高级设置"), this);
    auto* collectLayout = new QVBoxLayout(collectGroup);
    collectPanel_ = new CollectPanel(dev, CollectPanel::Full, collectGroup);
    collectPanel_->setObjectName(QStringLiteral("advancedCollectPanel"));
    collectLayout->addWidget(collectPanel_);
    root->addWidget(collectGroup);

    root->addStretch();
}

#include "AdvancedSettingsPanel.h"

#include "CameraPanel.h"
#include "CollectPanel.h"
#include "ConnectionPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
const QString kSettingsPrefix = QStringLiteral("panels/advanced/");
}

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

    auto* stretchCropGroup = new QGroupBox(QString::fromUtf8("Raw16 拉伸裁剪"), this);
    stretchCropGroup->setObjectName(QStringLiteral("rawStretchCropGroup"));
    auto* stretchCropLayout = new QFormLayout(stretchCropGroup);
    auto* hint = new QLabel(
        QString::fromUtf8("先将 Raw16 水平拉伸到目标宽度，再从左侧裁掉指定列数；右侧自动裁剪，最终尺寸保持原始大小。目标宽度为 0 时关闭，左裁超过可裁范围时自动限制。"),
        stretchCropGroup);
    hint->setObjectName(QStringLiteral("rawStretchCropHint"));
    hint->setWordWrap(true);
    stretchCropLayout->addRow(hint);

    rawStretchWidthSpin_ = new QSpinBox(stretchCropGroup);
    rawStretchWidthSpin_->setObjectName(QStringLiteral("rawStretchWidthSpin"));
    rawStretchWidthSpin_->setRange(0, 100000);
    rawStretchWidthSpin_->setSpecialValueText(QString::fromUtf8("关闭"));
    rawStretchWidthSpin_->setSuffix(QStringLiteral(" px"));
    stretchCropLayout->addRow(QString::fromUtf8("拉伸目标宽度"), rawStretchWidthSpin_);

    rawCropLeftColumnsSpin_ = new QSpinBox(stretchCropGroup);
    rawCropLeftColumnsSpin_->setObjectName(QStringLiteral("rawCropLeftColumnsSpin"));
    rawCropLeftColumnsSpin_->setRange(0, 100000);
    rawCropLeftColumnsSpin_->setSuffix(QString::fromUtf8(" 列"));
    stretchCropLayout->addRow(QString::fromUtf8("左侧裁剪列数"), rawCropLeftColumnsSpin_);

    QSettings settings;
    rawStretchWidthSpin_->setValue(
        settings.value(kSettingsPrefix + QStringLiteral("rawStretchWidth"), 0).toInt());
    rawCropLeftColumnsSpin_->setValue(
        settings.value(kSettingsPrefix + QStringLiteral("rawCropLeftColumns"), 0).toInt());

    const auto saveAndNotify = [this]() {
        QSettings s;
        s.setValue(kSettingsPrefix + QStringLiteral("rawStretchWidth"), rawStretchWidth());
        s.setValue(kSettingsPrefix + QStringLiteral("rawCropLeftColumns"), rawCropLeftColumns());
        emit rawStretchCropChanged(rawStretchWidth(), rawCropLeftColumns());
    };
    connect(rawStretchWidthSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [saveAndNotify](int) { saveAndNotify(); });
    connect(rawCropLeftColumnsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [saveAndNotify](int) { saveAndNotify(); });
    root->addWidget(stretchCropGroup);

    auto* collectGroup = new QGroupBox(QString::fromUtf8("采集高级设置"), this);
    auto* collectLayout = new QVBoxLayout(collectGroup);
    collectPanel_ = new CollectPanel(dev, CollectPanel::Full, collectGroup);
    collectPanel_->setObjectName(QStringLiteral("advancedCollectPanel"));
    collectLayout->addWidget(collectPanel_);
    root->addWidget(collectGroup);

    root->addStretch();
}

int AdvancedSettingsPanel::rawStretchWidth() const
{
    return rawStretchWidthSpin_ ? rawStretchWidthSpin_->value() : 0;
}

int AdvancedSettingsPanel::rawCropLeftColumns() const
{
    return rawCropLeftColumnsSpin_ ? rawCropLeftColumnsSpin_->value() : 0;
}

#include "IrPanel.h"
#include "DeviceClient.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

IrPanel::IrPanel(DeviceClient* dev, QWidget* parent)
    : QWidget(parent), dev_(dev)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    // brightness / contrast / integration
    auto* paramGrp = new QGroupBox(QString::fromUtf8("亮度 / 对比度 / 积分"), this);
    auto* paramForm = new QFormLayout(paramGrp);

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

    paramForm->addRow(QString::fromUtf8("亮度"), makeSliderRow(brightSlider_, brightSpin_, 255));
    paramForm->addRow(QString::fromUtf8("对比度"), makeSliderRow(contrastSlider_, contrastSpin_, 255));

    integSpin_ = new QSpinBox(this);
    integSpin_->setRange(0, 65535);
    paramForm->addRow(QString::fromUtf8("积分时间"), integSpin_);

    auto* applyBright   = new QPushButton(QString::fromUtf8("设亮度"), this);
    auto* applyContrast = new QPushButton(QString::fromUtf8("设对比"), this);
    auto* applyInteg    = new QPushButton(QString::fromUtf8("设积分"), this);
    auto* applyRow = new QHBoxLayout();
    applyRow->addWidget(applyBright);
    applyRow->addWidget(applyContrast);
    applyRow->addWidget(applyInteg);
    paramForm->addRow(applyRow);
    root->addWidget(paramGrp);

    connect(applyBright, &QPushButton::clicked, this, [this]() {
        dev_->irSetBrightness(static_cast<quint8>(brightSpin_->value()),
            [this](bool ok, const QString& err) { if (!ok) QMessageBox::warning(this, "IR", err); });
    });
    connect(applyContrast, &QPushButton::clicked, this, [this]() {
        dev_->irSetContrast(static_cast<quint8>(contrastSpin_->value()),
            [this](bool ok, const QString& err) { if (!ok) QMessageBox::warning(this, "IR", err); });
    });
    connect(applyInteg, &QPushButton::clicked, this, [this]() {
        dev_->irSetIntegration(static_cast<quint16>(integSpin_->value()),
            [this](bool ok, const QString& err) { if (!ok) QMessageBox::warning(this, "IR", err); });
    });

    // query buttons
    auto* queryGrp = new QGroupBox(QString::fromUtf8("查询与操作"), this);
    auto* queryVb = new QVBoxLayout(queryGrp);

    resultLabel_ = new QLabel("-", this);
    resultLabel_->setWordWrap(true);
    resultLabel_->setProperty("secondary", true);

    auto* calibBtn    = new QPushButton(QString::fromUtf8("触发标定"), this);
    auto* queryIntBtn = new QPushButton(QString::fromUtf8("查询积分时间"), this);
    auto* selfChkBtn  = new QPushButton(QString::fromUtf8("自检"), this);
    auto* coreTempBtn = new QPushButton(QString::fromUtf8("读机芯温度"), this);
    auto* focusTempBtn = new QPushButton(QString::fromUtf8("读焦面温度"), this);
    auto* modIdBtn    = new QPushButton(QString::fromUtf8("读模组 ID"), this);

    queryVb->addWidget(calibBtn);
    queryVb->addWidget(queryIntBtn);
    queryVb->addWidget(selfChkBtn);
    queryVb->addWidget(coreTempBtn);
    queryVb->addWidget(focusTempBtn);
    queryVb->addWidget(modIdBtn);
    queryVb->addWidget(resultLabel_);
    root->addWidget(queryGrp);

    auto showCb = [this](const QString& title) {
        return [this, title](bool ok, const nlohmann::json& data, const QString& err) {
            showResult(title, ok, data, err);
        };
    };

    connect(calibBtn, &QPushButton::clicked, this, [this]() {
        dev_->irTriggerCalibration([this](bool ok, const QString& err) {
            resultLabel_->setText(ok ? "OK" : err);
        });
    });
    connect(queryIntBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->irQueryIntegrationTime(showCb(QString::fromUtf8("积分时间")));
    });
    connect(selfChkBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->irReadSelfCheck(showCb(QString::fromUtf8("自检")));
    });
    connect(coreTempBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->irReadCoreTemp(showCb(QString::fromUtf8("机芯温度")));
    });
    connect(focusTempBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->irReadFocusPlaneTemp(showCb(QString::fromUtf8("焦面温度")));
    });
    connect(modIdBtn, &QPushButton::clicked, this, [this, showCb]() {
        dev_->irReadModuleId(showCb(QString::fromUtf8("模组 ID")));
    });

    // raw command
    auto* rawGrp = new QGroupBox(QString::fromUtf8("原始命令"), this);
    auto* rawForm = new QFormLayout(rawGrp);
    rawCmdEdit_ = new QLineEdit("02", this);
    rawDataEdit_ = new QLineEdit("01", this);
    rawLenSpin_ = new QSpinBox(this);
    rawLenSpin_->setRange(0, 255);
    rawLenSpin_->setValue(1);
    rawResultLabel_ = new QLabel("-", this);
    rawResultLabel_->setWordWrap(true);
    rawResultLabel_->setProperty("secondary", true);

    rawForm->addRow("CMD (hex)", rawCmdEdit_);
    rawForm->addRow("DATA (hex)", rawDataEdit_);
    rawForm->addRow("Readback Len", rawLenSpin_);

    auto* sendBtn = new QPushButton(QString::fromUtf8("发送"), this);
    rawForm->addRow(sendBtn);
    rawForm->addRow(rawResultLabel_);
    root->addWidget(rawGrp);

    connect(sendBtn, &QPushButton::clicked, this, [this]() {
        bool ok;
        quint8 cmd = static_cast<quint8>(rawCmdEdit_->text().toUInt(&ok, 16));
        if (!ok) { QMessageBox::warning(this, "IR", "Invalid CMD hex"); return; }
        QByteArray data = QByteArray::fromHex(rawDataEdit_->text().toLatin1());
        quint8 len = static_cast<quint8>(rawLenSpin_->value());
        dev_->irSendRaw(cmd, data, len, [this](bool ok2, const nlohmann::json& d, const QString& err) {
            if (ok2)
                rawResultLabel_->setText("cmd=" + QString::number(d.value("cmd", 0)) + " data=" + bytesToHex(d["data"]));
            else
                rawResultLabel_->setText(err);
        });
    });

    root->addStretch();
}

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

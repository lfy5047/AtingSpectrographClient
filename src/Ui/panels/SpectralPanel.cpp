#include "SpectralPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Protocol.h"

SpectralPanel::SpectralPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* modeGroup = new QGroupBox(QString::fromUtf8("光谱显示"), this);
    auto* form = new QFormLayout(modeGroup);

    sourceModeCombo_ = new QComboBox(this);
    sourceModeCombo_->addItem("Auto", static_cast<int>(SpectralSourceMode::Auto));
    sourceModeCombo_->addItem("Live", static_cast<int>(SpectralSourceMode::Live));
    form->addRow("Stream Source", sourceModeCombo_);

    sourceCombo_ = new QComboBox(this);
    sourceCombo_->addItem("SliceStitch16", static_cast<int>(cli::proto::SliceStitch16));
    sourceCombo_->addItem("Raw16", static_cast<int>(cli::proto::Raw16));
    form->addRow(QString::fromUtf8("来源通道"), sourceCombo_);

    modeCombo_ = new QComboBox(this);
    modeCombo_->addItem(QString::fromUtf8("单波段"), static_cast<int>(SpectralRenderOptions::SingleBand));
    modeCombo_->addItem(QString::fromUtf8("范围平均"), static_cast<int>(SpectralRenderOptions::RangeAverage));
    modeCombo_->addItem(QString::fromUtf8("RGB 合成"), static_cast<int>(SpectralRenderOptions::RgbComposite));
    form->addRow(QString::fromUtf8("显示模式"), modeCombo_);

    singleBandSpin_ = new QSpinBox(this);
    singleBandSpin_->setRange(0, 0);
    form->addRow(QString::fromUtf8("波段"), singleBandSpin_);

    rangeStartSpin_ = new QSpinBox(this);
    rangeStartSpin_->setRange(0, 0);
    form->addRow(QString::fromUtf8("起始波段"), rangeStartSpin_);

    rangeEndSpin_ = new QSpinBox(this);
    rangeEndSpin_->setRange(0, 0);
    form->addRow(QString::fromUtf8("结束波段"), rangeEndSpin_);

    rBandSpin_ = new QSpinBox(this);
    rBandSpin_->setRange(0, 0);
    form->addRow("R Band", rBandSpin_);

    gBandSpin_ = new QSpinBox(this);
    gBandSpin_->setRange(0, 0);
    form->addRow("G Band", gBandSpin_);

    bBandSpin_ = new QSpinBox(this);
    bBandSpin_->setRange(0, 0);
    form->addRow("B Band", bBandSpin_);

    root->addWidget(modeGroup);

    auto* statsGroup = new QGroupBox(QString::fromUtf8("扫描状态"), this);
    auto* statsLayout = new QVBoxLayout(statsGroup);
    statsLabel_ = new QLabel("-", this);
    statsLabel_->setWordWrap(true);
    statsLayout->addWidget(statsLabel_);
    root->addWidget(statsGroup);
    root->addStretch();

    connect(sourceModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectralPanel::settingsChanged);
    connect(sourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectralPanel::settingsChanged);
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectralPanel::updateModeVisibility);
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectralPanel::settingsChanged);
    connect(singleBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpectralPanel::settingsChanged);
    connect(rangeStartSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpectralPanel::settingsChanged);
    connect(rangeEndSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpectralPanel::settingsChanged);
    connect(rBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpectralPanel::settingsChanged);
    connect(gBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpectralPanel::settingsChanged);
    connect(bBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &SpectralPanel::settingsChanged);

    updateModeVisibility();
}

SpectralSourceMode SpectralPanel::sourceMode() const
{
    return static_cast<SpectralSourceMode>(sourceModeCombo_->currentData().toInt());
}

int SpectralPanel::sourceChannel() const
{
    return sourceCombo_->currentData().toInt();
}

SpectralRenderOptions SpectralPanel::renderOptions() const
{
    SpectralRenderOptions opts;
    opts.mode = static_cast<SpectralRenderOptions::Mode>(modeCombo_->currentData().toInt());
    opts.singleBand = singleBandSpin_->value();
    opts.rangeStart = rangeStartSpin_->value();
    opts.rangeEnd = rangeEndSpin_->value();
    opts.rBand = rBandSpin_->value();
    opts.gBand = gBandSpin_->value();
    opts.bBand = bBandSpin_->value();
    return opts;
}

void SpectralPanel::setBandCount(int bands)
{
    const int maxBand = bands > 0 ? bands - 1 : 0;
    singleBandSpin_->setRange(0, maxBand);
    rangeStartSpin_->setRange(0, maxBand);
    rangeEndSpin_->setRange(0, maxBand);
    rBandSpin_->setRange(0, maxBand);
    gBandSpin_->setRange(0, maxBand);
    bBandSpin_->setRange(0, maxBand);
}

void SpectralPanel::setStats(const QString& activeSource, int scanWidth, int height, int bands,
                             bool tailSeen, bool active, quint64 gapFillColumns)
{
    statsLabel_->setText(QString("source=%1\ncols=%2\nheight=%3\nbands=%4\ntail=%5\nactive=%6\ngapFill=%7")
                             .arg(activeSource)
                             .arg(scanWidth)
                             .arg(height)
                             .arg(bands)
                             .arg(tailSeen ? "yes" : "no")
                             .arg(active ? "yes" : "no")
                             .arg(gapFillColumns));
}

void SpectralPanel::updateModeVisibility()
{
    const auto mode = static_cast<SpectralRenderOptions::Mode>(modeCombo_->currentData().toInt());
    const bool single = mode == SpectralRenderOptions::SingleBand;
    const bool range = mode == SpectralRenderOptions::RangeAverage;
    const bool rgb = mode == SpectralRenderOptions::RgbComposite;

    auto* fl = qobject_cast<QFormLayout*>(singleBandSpin_->parentWidget()->layout());
    if (!fl) return;

    auto setFieldVisible = [fl](QWidget* field, bool visible) {
        if (!field) return;
        field->setVisible(visible);
        if (QLabel* label = qobject_cast<QLabel*>(fl->labelForField(field))) {
            label->setVisible(visible);
        }
    };

    setFieldVisible(singleBandSpin_, single);
    setFieldVisible(rangeStartSpin_, range);
    setFieldVisible(rangeEndSpin_, range);
    setFieldVisible(rBandSpin_, rgb);
    setFieldVisible(gBandSpin_, rgb);
    setFieldVisible(bBandSpin_, rgb);
}

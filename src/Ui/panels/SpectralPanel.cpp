#include "SpectralPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "PanelSettings.h"
#include "Protocol.h"

SpectralPanel::SpectralPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* modeGroup = new QGroupBox(QString::fromUtf8("光谱显示"), this);
    auto* form = new QFormLayout(modeGroup);

    sourceModeCombo_ = new QComboBox(this);
    sourceModeCombo_->setObjectName(QStringLiteral("spectralSourceModeCombo"));
    sourceModeCombo_->addItem("Auto", static_cast<int>(SpectralSourceMode::Auto));
    sourceModeCombo_->addItem("Live", static_cast<int>(SpectralSourceMode::Live));
    form->addRow("Stream Source", sourceModeCombo_);

    sourceCombo_ = new QComboBox(this);
    sourceCombo_->setObjectName(QStringLiteral("spectralSourceCombo"));
    sourceCombo_->addItem("SliceStitch16", static_cast<int>(cli::proto::SliceStitch16));
    sourceCombo_->addItem("Raw16", static_cast<int>(cli::proto::Raw16));
    form->addRow(QString::fromUtf8("来源通道"), sourceCombo_);

    modeCombo_ = new QComboBox(this);
    modeCombo_->setObjectName(QStringLiteral("spectralModeCombo"));
    modeCombo_->addItem(QString::fromUtf8("单波段"), static_cast<int>(SpectralRenderOptions::SingleBand));
    modeCombo_->addItem(QString::fromUtf8("范围平均"), static_cast<int>(SpectralRenderOptions::RangeAverage));
    modeCombo_->addItem(QString::fromUtf8("RGB 合成"), static_cast<int>(SpectralRenderOptions::RgbComposite));
    form->addRow(QString::fromUtf8("显示模式"), modeCombo_);

    singleBandSpin_ = new QSpinBox(this);
    singleBandSpin_->setObjectName(QStringLiteral("spectralSingleBandSpin"));
    singleBandSpin_->setRange(0, 0);
    form->addRow(QString::fromUtf8("波段"), singleBandSpin_);

    rangeStartSpin_ = new QSpinBox(this);
    rangeStartSpin_->setObjectName(QStringLiteral("spectralRangeStartSpin"));
    rangeStartSpin_->setRange(0, 0);
    form->addRow(QString::fromUtf8("起始波段"), rangeStartSpin_);

    rangeEndSpin_ = new QSpinBox(this);
    rangeEndSpin_->setObjectName(QStringLiteral("spectralRangeEndSpin"));
    rangeEndSpin_->setRange(0, 0);
    form->addRow(QString::fromUtf8("结束波段"), rangeEndSpin_);

    rBandSpin_ = new QSpinBox(this);
    rBandSpin_->setObjectName(QStringLiteral("spectralRBandSpin"));
    rBandSpin_->setRange(0, 0);
    form->addRow("R Band", rBandSpin_);

    gBandSpin_ = new QSpinBox(this);
    gBandSpin_->setObjectName(QStringLiteral("spectralGBandSpin"));
    gBandSpin_->setRange(0, 0);
    form->addRow("G Band", gBandSpin_);

    bBandSpin_ = new QSpinBox(this);
    bBandSpin_->setObjectName(QStringLiteral("spectralBBandSpin"));
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

    loadSettings();

    connect(sourceModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
        emit settingsChanged();
    });
    connect(sourceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
        emit settingsChanged();
    });
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectralPanel::updateModeVisibility);
    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        saveSettings();
        emit settingsChanged();
    });
    auto saveAndEmit = [this](int) {
        saveSettings();
        emit settingsChanged();
    };
    connect(singleBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, saveAndEmit);
    connect(rangeStartSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, saveAndEmit);
    connect(rangeEndSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, saveAndEmit);
    connect(rBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, saveAndEmit);
    connect(gBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, saveAndEmit);
    connect(bBandSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, saveAndEmit);

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
    applySavedBandSettings();
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

void SpectralPanel::loadSettings()
{
    QSettings s;
    const QString p = QStringLiteral("panels/spectral/");
    PanelSettings::setComboByData(sourceModeCombo_, s.value(p + QStringLiteral("sourceMode"), sourceModeCombo_->currentData()), 0);
    PanelSettings::setComboByData(sourceCombo_, s.value(p + QStringLiteral("sourceChannel"), sourceCombo_->currentData()), 0);
    PanelSettings::setComboByData(modeCombo_, s.value(p + QStringLiteral("mode"), modeCombo_->currentData()), 0);
    applySavedBandSettings();
}

void SpectralPanel::applySavedBandSettings()
{
    QSettings s;
    const QString p = QStringLiteral("panels/spectral/");
    const QSignalBlocker blockSingle(singleBandSpin_);
    const QSignalBlocker blockRangeStart(rangeStartSpin_);
    const QSignalBlocker blockRangeEnd(rangeEndSpin_);
    const QSignalBlocker blockR(rBandSpin_);
    const QSignalBlocker blockG(gBandSpin_);
    const QSignalBlocker blockB(bBandSpin_);
    if (s.contains(p + QStringLiteral("singleBand")))
        singleBandSpin_->setValue(s.value(p + QStringLiteral("singleBand")).toInt());
    if (s.contains(p + QStringLiteral("rangeStart")))
        rangeStartSpin_->setValue(s.value(p + QStringLiteral("rangeStart")).toInt());
    if (s.contains(p + QStringLiteral("rangeEnd")))
        rangeEndSpin_->setValue(s.value(p + QStringLiteral("rangeEnd")).toInt());
    if (s.contains(p + QStringLiteral("rBand")))
        rBandSpin_->setValue(s.value(p + QStringLiteral("rBand")).toInt());
    if (s.contains(p + QStringLiteral("gBand")))
        gBandSpin_->setValue(s.value(p + QStringLiteral("gBand")).toInt());
    if (s.contains(p + QStringLiteral("bBand")))
        bBandSpin_->setValue(s.value(p + QStringLiteral("bBand")).toInt());
}

void SpectralPanel::saveSettings() const
{
    QSettings s;
    const QString p = QStringLiteral("panels/spectral/");
    s.setValue(p + QStringLiteral("sourceMode"), sourceModeCombo_->currentData());
    s.setValue(p + QStringLiteral("sourceChannel"), sourceCombo_->currentData());
    s.setValue(p + QStringLiteral("mode"), modeCombo_->currentData());
    s.setValue(p + QStringLiteral("singleBand"), singleBandSpin_->value());
    s.setValue(p + QStringLiteral("rangeStart"), rangeStartSpin_->value());
    s.setValue(p + QStringLiteral("rangeEnd"), rangeEndSpin_->value());
    s.setValue(p + QStringLiteral("rBand"), rBandSpin_->value());
    s.setValue(p + QStringLiteral("gBand"), gBandSpin_->value());
    s.setValue(p + QStringLiteral("bBand"), bBandSpin_->value());
}

#include "SpectrumAnalysisPanel.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
const char* kSettingsPrefix = "spectrumAnalysis/";
const int kMaxLines = 16;

int oddWindowValue(int value)
{
    value = qBound(1, value, 101);
    if (value % 2 == 0) {
        value = value < 101 ? value + 1 : value - 1;
    }
    return value;
}

QVector<QColor> linePalette()
{
    return {
        QColor("#4C8EF7"), QColor("#F97316"), QColor("#22C55E"), QColor("#E5484D"),
        QColor("#A855F7"), QColor("#14B8A6"), QColor("#FACC15"), QColor("#EC4899"),
        QColor("#38BDF8"), QColor("#84CC16"), QColor("#FB7185"), QColor("#C084FC"),
        QColor("#2DD4BF"), QColor("#F59E0B"), QColor("#60A5FA"), QColor("#F472B6"),
    };
}

QIcon colorIcon(const QColor& color)
{
    QPixmap pm(18, 18);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(2, 4, 14, 10, 3, 3);
    return QIcon(pm);
}
}

SpectrumAnalysisPanel::SpectrumAnalysisPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto* mappingGroup = new QGroupBox(QString::fromUtf8("波长映射"), this);
    auto* mappingForm = new QFormLayout(mappingGroup);

    xStartSpin_ = new QSpinBox(this);
    xEndSpin_ = new QSpinBox(this);
    xStartSpin_->setRange(0, 1000000);
    xEndSpin_->setRange(0, 1000000);
    mappingForm->addRow(QString::fromUtf8("X 起点"), xStartSpin_);
    mappingForm->addRow(QString::fromUtf8("X 终点"), xEndSpin_);

    wavelengthStartSpin_ = new QDoubleSpinBox(this);
    wavelengthEndSpin_ = new QDoubleSpinBox(this);
    wavelengthStartSpin_->setDecimals(4);
    wavelengthEndSpin_->setDecimals(4);
    wavelengthStartSpin_->setRange(-1000000.0, 1000000.0);
    wavelengthEndSpin_->setRange(-1000000.0, 1000000.0);
    wavelengthStartSpin_->setSingleStep(0.1);
    wavelengthEndSpin_->setSingleStep(0.1);
    mappingForm->addRow(QString::fromUtf8("波长起点"), wavelengthStartSpin_);
    mappingForm->addRow(QString::fromUtf8("波长终点"), wavelengthEndSpin_);

    refreshRateSpin_ = new QSpinBox(this);
    refreshRateSpin_->setRange(1, 30);
    refreshRateSpin_->setSuffix(" Hz");
    mappingForm->addRow(QString::fromUtf8("刷新率"), refreshRateSpin_);
    root->addWidget(mappingGroup);

    auto* lineGroup = new QGroupBox(QString::fromUtf8("水平采样线"), this);
    auto* lineLayout = new QVBoxLayout(lineGroup);

    auto* addRow = new QHBoxLayout();
    addYSpin_ = new QSpinBox(this);
    addYSpin_->setRange(0, 1000000);
    addButton_ = new QPushButton(QString::fromUtf8("添加"), this);
    addButton_->setProperty("primary", true);
    addRow->addWidget(new QLabel("Y", this));
    addRow->addWidget(addYSpin_, 1);
    addRow->addWidget(addButton_);
    lineLayout->addLayout(addRow);

    lineList_ = new QListWidget(this);
    lineList_->setSelectionMode(QAbstractItemView::SingleSelection);
    lineLayout->addWidget(lineList_, 1);

    auto* buttonRow = new QHBoxLayout();
    deleteButton_ = new QPushButton(QString::fromUtf8("删除选中"), this);
    deleteButton_->setProperty("danger", true);
    clearButton_ = new QPushButton(QString::fromUtf8("清空"), this);
    clearButton_->setProperty("danger", true);
    buttonRow->addWidget(deleteButton_);
    buttonRow->addWidget(clearButton_);
    lineLayout->addLayout(buttonRow);
    root->addWidget(lineGroup, 1);

    showDialogButton_ = new QPushButton(QString::fromUtf8("打开曲线窗口"), this);
    showDialogButton_->setProperty("primary", true);
    root->addWidget(showDialogButton_);

    auto* processingGroup = new QGroupBox(QString::fromUtf8("曲线处理"), this);
    auto* processingForm = new QFormLayout(processingGroup);

    filterWindowSpin_ = new QSpinBox(this);
    filterWindowSpin_->setRange(1, 101);
    filterWindowSpin_->setSingleStep(2);
    filterWindowSpin_->setSuffix(QString::fromUtf8(" px"));
    processingForm->addRow(QString::fromUtf8("移动平均窗口"), filterWindowSpin_);

    maxPlotPointsSpin_ = new QSpinBox(this);
    maxPlotPointsSpin_->setRange(20, 10000);
    maxPlotPointsSpin_->setSingleStep(20);
    maxPlotPointsSpin_->setSuffix(QString::fromUtf8(" 点"));
    processingForm->addRow(QString::fromUtf8("最大绘制点数"), maxPlotPointsSpin_);

    yRangeMultiplierSpin_ = new QDoubleSpinBox(this);
    yRangeMultiplierSpin_->setRange(1.05, 10.0);
    yRangeMultiplierSpin_->setDecimals(2);
    yRangeMultiplierSpin_->setSingleStep(0.05);
    yRangeMultiplierSpin_->setSuffix(QString::fromUtf8(" x"));
    processingForm->addRow(QString::fromUtf8("Y轴范围倍率"), yRangeMultiplierSpin_);

    yMinPositionPercentSpin_ = new QDoubleSpinBox(this);
    yMinPositionPercentSpin_->setRange(0.0, 30.0);
    yMinPositionPercentSpin_->setDecimals(1);
    yMinPositionPercentSpin_->setSingleStep(0.5);
    yMinPositionPercentSpin_->setSuffix(QString::fromUtf8(" %"));
    processingForm->addRow(QString::fromUtf8("最小值位置"), yMinPositionPercentSpin_);

    yMinDataSpanSpin_ = new QDoubleSpinBox(this);
    yMinDataSpanSpin_->setRange(1.0, 65535.0);
    yMinDataSpanSpin_->setDecimals(0);
    yMinDataSpanSpin_->setSingleStep(10.0);
    yMinDataSpanSpin_->setSuffix(QString::fromUtf8(" DN"));
    processingForm->addRow(QString::fromUtf8("最小数据跨度"), yMinDataSpanSpin_);
    root->addWidget(processingGroup);

    statusLabel_ = new QLabel(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"), this);
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    connect(xStartSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (loadingSettings_) return;
        if (xStartSpin_->value() >= xEndSpin_->value()) {
            const QSignalBlocker blocker(xEndSpin_);
            xEndSpin_->setValue(qMin(xEndSpin_->maximum(), xStartSpin_->value() + 1));
        }
        saveSettings();
        emit settingsChanged();
    });
    connect(xEndSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (loadingSettings_) return;
        if (xEndSpin_->value() <= xStartSpin_->value()) {
            const QSignalBlocker blocker(xStartSpin_);
            xStartSpin_->setValue(qMax(xStartSpin_->minimum(), xEndSpin_->value() - 1));
        }
        saveSettings();
        emit settingsChanged();
    });
    connect(wavelengthStartSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(wavelengthEndSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(refreshRateSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(filterWindowSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (loadingSettings_) return;
        const int oddValue = oddWindowValue(value);
        if (value != oddValue) {
            const QSignalBlocker blocker(filterWindowSpin_);
            filterWindowSpin_->setValue(oddValue);
        }
        saveSettings();
        emit settingsChanged();
    });
    connect(maxPlotPointsSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(yRangeMultiplierSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(yMinPositionPercentSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(yMinDataSpanSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
        if (loadingSettings_) return;
        saveSettings();
        emit settingsChanged();
    });
    connect(addButton_, &QPushButton::clicked, this, [this]() {
        addLineAt(addYSpin_->value());
    });
    connect(deleteButton_, &QPushButton::clicked, this, [this]() {
        deleteLine(lineList_->currentRow());
    });
    connect(clearButton_, &QPushButton::clicked, this, [this]() {
        if (lines_.isEmpty()) return;
        lines_.clear();
        refreshLineList();
        saveSettings();
        emit linesChanged();
    });
    connect(showDialogButton_, &QPushButton::clicked, this, &SpectrumAnalysisPanel::showDialogRequested);

    loadSettings();
    updateRanges();
    refreshLineList();
}

int SpectrumAnalysisPanel::xStart() const { return xStartSpin_->value(); }
int SpectrumAnalysisPanel::xEnd() const { return xEndSpin_->value(); }
double SpectrumAnalysisPanel::wavelengthStart() const { return wavelengthStartSpin_->value(); }
double SpectrumAnalysisPanel::wavelengthEnd() const { return wavelengthEndSpin_->value(); }
int SpectrumAnalysisPanel::refreshRateHz() const { return refreshRateSpin_->value(); }
int SpectrumAnalysisPanel::filterWindowPixels() const { return oddWindowValue(filterWindowSpin_->value()); }
int SpectrumAnalysisPanel::maxPlotPoints() const { return maxPlotPointsSpin_->value(); }
double SpectrumAnalysisPanel::yRangeMultiplier() const { return yRangeMultiplierSpin_->value(); }
double SpectrumAnalysisPanel::yMinPositionPercent() const { return yMinPositionPercentSpin_->value(); }
double SpectrumAnalysisPanel::yMinDataSpan() const { return yMinDataSpanSpin_->value(); }

void SpectrumAnalysisPanel::setSliceGeometry(int width, int height, bool valid)
{
    const bool changed = sliceWidth_ != width || sliceHeight_ != height || sliceValid_ != valid;
    sliceWidth_ = qMax(0, width);
    sliceHeight_ = qMax(0, height);
    sliceValid_ = valid && sliceWidth_ > 0 && sliceHeight_ > 0;
    updateRanges();
    if (changed) {
        bool moved = false;
        for (auto& line : lines_) {
            const int oldY = line.y;
            line.y = clampY(line.y);
            moved = moved || oldY != line.y;
        }
        if (moved) {
            refreshLineList();
            saveSettings();
            emit linesChanged();
        }
    }
}

bool SpectrumAnalysisPanel::addLineAt(int y)
{
    if (!sliceValid_) {
        setStatusText(QString::fromUtf8("等待 SliceStitch16 Mono16 数据"));
        return false;
    }
    if (lines_.size() >= kMaxLines) {
        setStatusText(QString::fromUtf8("最多支持 16 条采样线"));
        return false;
    }

    SpectrumSampleLine line;
    line.y = clampY(y);
    line.color = nextLineColor();
    lines_.append(line);
    refreshLineList();
    saveSettings();
    setStatusText(QString::fromUtf8("已添加采样线 y=%1").arg(line.y));
    emit linesChanged();
    return true;
}

void SpectrumAnalysisPanel::moveLine(int index, int y)
{
    if (index < 0 || index >= lines_.size()) return;
    const int newY = clampY(y);
    if (lines_[index].y == newY) return;
    lines_[index].y = newY;
    refreshLineList();
    lineList_->setCurrentRow(index);
    saveSettings();
    emit linesChanged();
}

void SpectrumAnalysisPanel::deleteLine(int index)
{
    if (index < 0 || index >= lines_.size()) return;
    lines_.remove(index);
    refreshLineList();
    saveSettings();
    emit linesChanged();
}

void SpectrumAnalysisPanel::setStatusText(const QString& text)
{
    if (statusLabel_) statusLabel_->setText(text);
}

void SpectrumAnalysisPanel::loadSettings()
{
    loadingSettings_ = true;
    QSettings s;
    xStartSpin_->setValue(s.value(QString(kSettingsPrefix) + "xStart", 0).toInt());
    xEndSpin_->setValue(s.value(QString(kSettingsPrefix) + "xEnd", 0).toInt());
    wavelengthStartSpin_->setValue(s.value(QString(kSettingsPrefix) + "wavelengthStart", 7.0).toDouble());
    wavelengthEndSpin_->setValue(s.value(QString(kSettingsPrefix) + "wavelengthEnd", 12.5).toDouble());
    refreshRateSpin_->setValue(s.value(QString(kSettingsPrefix) + "refreshRateHz", 10).toInt());
    filterWindowSpin_->setValue(oddWindowValue(s.value(QString(kSettingsPrefix) + "filterWindowPixels", 5).toInt()));
    maxPlotPointsSpin_->setValue(s.value(QString(kSettingsPrefix) + "maxPlotPoints", 100).toInt());
    yRangeMultiplierSpin_->setValue(s.value(QString(kSettingsPrefix) + "yRangeMultiplier", 1.3).toDouble());
    yMinPositionPercentSpin_->setValue(s.value(QString(kSettingsPrefix) + "yMinPositionPercent", 3.0).toDouble());
    yMinDataSpanSpin_->setValue(s.value(QString(kSettingsPrefix) + "yMinDataSpan", 100.0).toDouble());

    lines_.clear();
    const int count = qMin(kMaxLines, s.value(QString(kSettingsPrefix) + "lineCount", 0).toInt());
    const QVector<QColor> colors = linePalette();
    for (int i = 0; i < count; ++i) {
        SpectrumSampleLine line;
        const QString base = QString(kSettingsPrefix) + QString("lines/%1/").arg(i);
        line.y = s.value(base + "y", 0).toInt();
        line.color = QColor(s.value(base + "color", colors[i % colors.size()].name()).toString());
        lines_.append(line);
    }
    loadingSettings_ = false;
}

void SpectrumAnalysisPanel::saveSettings() const
{
    QSettings s;
    s.setValue(QString(kSettingsPrefix) + "xStart", xStartSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "xEnd", xEndSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "wavelengthStart", wavelengthStartSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "wavelengthEnd", wavelengthEndSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "refreshRateHz", refreshRateSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "filterWindowPixels", oddWindowValue(filterWindowSpin_->value()));
    s.setValue(QString(kSettingsPrefix) + "maxPlotPoints", maxPlotPointsSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "yRangeMultiplier", yRangeMultiplierSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "yMinPositionPercent", yMinPositionPercentSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "yMinDataSpan", yMinDataSpanSpin_->value());
    s.setValue(QString(kSettingsPrefix) + "lineCount", lines_.size());
    for (int i = 0; i < lines_.size(); ++i) {
        const QString base = QString(kSettingsPrefix) + QString("lines/%1/").arg(i);
        s.setValue(base + "y", lines_[i].y);
        s.setValue(base + "color", lines_[i].color.name());
    }
}

void SpectrumAnalysisPanel::refreshLineList()
{
    lineList_->clear();
    for (int i = 0; i < lines_.size(); ++i) {
        auto* item = new QListWidgetItem(colorIcon(lines_[i].color), lines_[i].name(), lineList_);
        item->setData(Qt::UserRole, i);
    }
    const bool hasLines = !lines_.isEmpty();
    deleteButton_->setEnabled(hasLines);
    clearButton_->setEnabled(hasLines);
}

void SpectrumAnalysisPanel::updateRanges()
{
    const int maxX = sliceValid_ ? qMax(0, sliceWidth_ - 1) : 1000000;
    const int maxY = sliceValid_ ? qMax(0, sliceHeight_ - 1) : 0;
    const QSignalBlocker bx0(xStartSpin_);
    const QSignalBlocker bx1(xEndSpin_);
    const QSignalBlocker by(addYSpin_);
    xStartSpin_->setRange(0, maxX);
    xEndSpin_->setRange(0, maxX);
    addYSpin_->setRange(0, maxY);
    if (sliceValid_) {
        if (xStartSpin_->value() > maxX) xStartSpin_->setValue(0);
        if (xEndSpin_->value() > maxX) xEndSpin_->setValue(maxX);
        if (xStartSpin_->value() >= xEndSpin_->value()) {
            xStartSpin_->setValue(0);
            xEndSpin_->setValue(maxX);
        }
    } else {
        addYSpin_->setValue(0);
    }
}

QColor SpectrumAnalysisPanel::nextLineColor() const
{
    const QVector<QColor> colors = linePalette();
    return colors[lines_.size() % colors.size()];
}

int SpectrumAnalysisPanel::clampY(int y) const
{
    const int maxY = sliceValid_ ? qMax(0, sliceHeight_ - 1) : 0;
    return qBound(0, y, maxY);
}

#pragma once

#include <QWidget>
#include <QVector>

#include "SpectrumAnalysisTypes.h"

class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

class SpectrumAnalysisPanel : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumAnalysisPanel(QWidget* parent = nullptr);

    QVector<SpectrumSampleLine> lines() const { return lines_; }
    int xStart() const;
    int xEnd() const;
    double wavelengthStart() const;
    double wavelengthEnd() const;
    int refreshRateHz() const;
    int filterWindowPixels() const;
    int maxPlotPoints() const;
    double yRangeMultiplier() const;
    double yMinPositionPercent() const;
    double yMinDataSpan() const;

    void setSliceGeometry(int width, int height, bool valid);
    bool addLineAt(int y);
    void moveLine(int index, int y);
    void deleteLine(int index);
    void setStatusText(const QString& text);

signals:
    void settingsChanged();
    void linesChanged();
    void showDialogRequested();

private:
    void loadSettings();
    void saveSettings() const;
    void refreshLineList();
    void updateRanges();
    QColor nextLineColor() const;
    int clampY(int y) const;

    QSpinBox* xStartSpin_ = nullptr;
    QSpinBox* xEndSpin_ = nullptr;
    QDoubleSpinBox* wavelengthStartSpin_ = nullptr;
    QDoubleSpinBox* wavelengthEndSpin_ = nullptr;
    QSpinBox* refreshRateSpin_ = nullptr;
    QSpinBox* filterWindowSpin_ = nullptr;
    QSpinBox* maxPlotPointsSpin_ = nullptr;
    QDoubleSpinBox* yRangeMultiplierSpin_ = nullptr;
    QDoubleSpinBox* yMinPositionPercentSpin_ = nullptr;
    QDoubleSpinBox* yMinDataSpanSpin_ = nullptr;
    QSpinBox* addYSpin_ = nullptr;
    QListWidget* lineList_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;
    QPushButton* showDialogButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QVector<SpectrumSampleLine> lines_;
    int sliceWidth_ = 0;
    int sliceHeight_ = 0;
    bool sliceValid_ = false;
    bool loadingSettings_ = false;
};

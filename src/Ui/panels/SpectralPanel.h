#pragma once

#include <QWidget>

#include "SpectralScanBuilder.h"

class QComboBox;
class QSpinBox;
class QLabel;
class QString;

enum class SpectralSourceMode {
    Auto = 0,
    Live = 1,
    Playback = 2,
};

class SpectralPanel : public QWidget {
    Q_OBJECT
public:
    explicit SpectralPanel(QWidget* parent = nullptr);

    SpectralSourceMode sourceMode() const;
    int sourceChannel() const;
    SpectralRenderOptions renderOptions() const;

    void setBandCount(int bands);
    void setStats(const QString& activeSource, int scanWidth, int height, int bands,
                  bool tailSeen, bool active, quint64 gapFillColumns);

signals:
    void settingsChanged();

private slots:
    void updateModeVisibility();

private:
    QComboBox* sourceModeCombo_ = nullptr;
    QComboBox* sourceCombo_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QSpinBox* singleBandSpin_ = nullptr;
    QSpinBox* rangeStartSpin_ = nullptr;
    QSpinBox* rangeEndSpin_ = nullptr;
    QSpinBox* rBandSpin_ = nullptr;
    QSpinBox* gBandSpin_ = nullptr;
    QSpinBox* bBandSpin_ = nullptr;
    QLabel* statsLabel_ = nullptr;
};

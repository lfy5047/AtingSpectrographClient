#pragma once

#include <QWidget>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>

class DeviceClient;

class MirrorPanel : public QWidget {
    Q_OBJECT
public:
    explicit MirrorPanel(DeviceClient* dev, QWidget* parent = nullptr);

private slots:
    void onQueryAngle();

private:
    void loadSettings();
    void saveSettings() const;

    DeviceClient* dev_;

    QLabel*          angleLabel_;
    QLabel*          movingLabel_;

    QDoubleSpinBox*  targetSpin_;
    QPushButton*     setTargetBtn_;
    QPushButton*     setAbsBtn_;
    QPushButton*     startBtn_;
    QPushButton*     stopBtn_;

    QSpinBox*        sSpeedSpin_;
    QSpinBox*        fSpeedSpin_;
    QPushButton*     applySpeedBtn_;

    QPushButton*     homeBtn_;
    QPushButton*     setHomeBtn_;

    QComboBox*       presetCombo_;
    QPushButton*     gotoPresetBtn_;

    QPushButton*     queryBtn_;
};

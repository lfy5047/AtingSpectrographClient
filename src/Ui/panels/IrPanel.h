#pragma once

#include <QWidget>
#include <QSlider>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include "json.hpp"

class DeviceClient;

class IrPanel : public QWidget {
    Q_OBJECT
public:
    explicit IrPanel(DeviceClient* dev, QWidget* parent = nullptr);

private:
    void showResult(const QString& title, bool ok, const nlohmann::json& data, const QString& err);
    static QString bytesToHex(const nlohmann::json& arr);

    DeviceClient* dev_;

    QSlider*   brightSlider_;
    QSpinBox*  brightSpin_;
    QSlider*   contrastSlider_;
    QSpinBox*  contrastSpin_;
    QSpinBox*  integSpin_;

    QLineEdit* rawCmdEdit_;
    QLineEdit* rawDataEdit_;
    QSpinBox*  rawLenSpin_;
    QLabel*    rawResultLabel_;

    QLabel*    resultLabel_;
};

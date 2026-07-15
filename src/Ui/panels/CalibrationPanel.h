#pragma once

#include <QWidget>

#include <QLabel>
#include <QPushButton>

class DeviceClient;
class ColumnNucPanel;
class QTimer;
struct BackgroundCalibrationStatus;

class CalibrationPanel : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationPanel(DeviceClient* dev, QWidget* parent = nullptr);

    void refreshColumnNuc();

private slots:
    void startBackgroundCalibration();
    void pollBackgroundCalibrationStatus();
    void finishBackgroundCalibration(const BackgroundCalibrationStatus& status);
    void setBackgroundCalibrationStage(const QString& stage, const QString& error = QString());

private:
    DeviceClient* dev_ = nullptr;
    QLabel* backgroundCalibrationStatusLabel_ = nullptr;
    QPushButton* backgroundCalibrationBtn_ = nullptr;
    QTimer* backgroundCalibrationTimer_ = nullptr;
    ColumnNucPanel* columnNucPanel_ = nullptr;
};

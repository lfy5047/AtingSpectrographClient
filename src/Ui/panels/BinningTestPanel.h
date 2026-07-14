#pragma once

#include "Client/core/DeviceTypes.h"

#include <QSize>
#include <QWidget>

class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;

class BinningTestPanel : public QWidget {
    Q_OBJECT
public:
    explicit BinningTestPanel(QWidget* parent = nullptr);

    int sourceChannel() const;
    void setCurrentConfig(const BinningConfig& config);
    void setBusy(bool busy, bool cancellable = true);
    void setStatusText(const QString& text, bool error = false);
    void setProgress(int completedSteps, const QString& text);
    void resetResults();
    void setCaptureResult(int factor, bool configVerified,
                          const QSize& expected, const QSize& actual);
    void setMeasurementResult(int factor, int pixels);

signals:
    void refreshRequested();
    void applyFactorRequested(int factor);
    void startRequested();
    void cancelRequested();
    void sourceChannelChanged(int channel);
    void measurementOrientationChanged(Qt::Orientation orientation);

private:
    static int rowForFactor(int factor);

    QComboBox* factorCombo_ = nullptr;
    QComboBox* sourceCombo_ = nullptr;
    QComboBox* orientationCombo_ = nullptr;
    QLabel* currentConfigLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
    QTableWidget* resultTable_ = nullptr;
};

#pragma once

#include "Client/core/DeviceTypes.h"

#include <QSize>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;

class RoiTestPanel : public QWidget {
    Q_OBJECT
public:
    explicit RoiTestPanel(QWidget* parent = nullptr);

    RoiConfig testConfig() const;
    void setCurrentConfig(const RoiConfig& config);
    void setResolution(const QSize& resolution);
    void setBusy(bool busy, bool cancellable = true);
    void setStatusText(const QString& text, bool error = false);
    void setProgress(int completedSteps, const QString& text);
    void resetResults();
    void setCaptureResult(bool fullFrame, const RoiConfig& config,
                          const QSize& expected, const QSize& actual);

signals:
    void refreshRequested();
    void applyRequested(RoiConfig config);
    void startRequested();
    void cancelRequested();

private:
    QSpinBox* sliceBeginSpin_ = nullptr;
    QSpinBox* sliceEndSpin_ = nullptr;
    QSpinBox* sliceHBeginSpin_ = nullptr;
    QSpinBox* sliceHEndSpin_ = nullptr;
    QLabel* currentConfigLabel_ = nullptr;
    QLabel* resolutionLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;
    QPushButton* applyBtn_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
    QTableWidget* resultTable_ = nullptr;
};

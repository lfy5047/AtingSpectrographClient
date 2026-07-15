#pragma once

#include "Client/core/DeviceTypes.h"

#include <QSize>
#include <QWidget>

class QLabel;
class QCheckBox;
class QProgressBar;
class QPushButton;
class QTableWidget;

class RoiTestPanel : public QWidget {
    Q_OBJECT
public:
    explicit RoiTestPanel(QWidget* parent = nullptr);

    bool applyWindowing() const;
    void setBusy(bool busy, bool cancellable = true);
    void setStatusText(const QString& text, bool error = false);
    void setProgress(int completedSteps, const QString& text);
    void resetResults();
    void setCaptureResult(bool fullFrame, const QSize& expected, const QSize& actual);

signals:
    void startRequested();
    void cancelRequested();

private:
    QCheckBox* applyWindowingCheck_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
    QTableWidget* resultTable_ = nullptr;
};

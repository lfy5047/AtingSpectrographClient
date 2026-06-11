#pragma once

#include <QDialog>
#include <QColor>
#include <QString>
#include <QVector>

class QLabel;
class QTimer;
class QCustomPlot;

class SpectrumCurveDialog : public QDialog {
    Q_OBJECT
public:
    explicit SpectrumCurveDialog(QWidget* parent = nullptr);

    void setRefreshRateHz(int hz);
    void setStatusText(const QString& text);
    void setCurveData(const QVector<QVector<double>>& xList,
                      const QVector<QVector<double>>& yList,
                      const QVector<QString>& names,
                      const QVector<QColor>& colors,
                      const QString& xAxisLabel,
                      double yRangeMultiplier,
                      double yMinPositionPercent,
                      double yMinDataSpan);

signals:
    void sampleRefreshRequested(bool force);

protected:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void loadGeometry();
    void saveGeometrySetting() const;
    void applyPlotTheme();

    QLabel* statusLabel_ = nullptr;
    QCustomPlot* plot_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    bool geometryLoaded_ = false;
};

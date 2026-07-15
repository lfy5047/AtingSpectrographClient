#pragma once

#include <QVector>
#include <QWidget>

#include "json.hpp"

class DeviceClient;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

class ColumnNucPanel : public QWidget {
    Q_OBJECT
public:
    explicit ColumnNucPanel(DeviceClient* device, QWidget* parent = nullptr);

    void refreshRemote();

private slots:
    void applyEnabledState();
    void reloadMatrices();
    void startLowCapture();
    void startHighCapture();
    void cancelCapture();
    void pollCaptureStatus();
    void refreshCaptures();
    void calibrateSelection();
    void updateSelectionState();

private:
    struct CaptureItem {
        QString captureId;
        quint64 timestampNs = 0;
        QString level;
        QString rawPath;
        QString jsonPath;
        int frameCount = 0;
        int width = 0;
        int height = 0;
        double temperature = 0.0;
        double actualTemperature = 0.0;
        bool hasActualTemperature = false;
        quint64 rawSizeBytes = 0;
        quint64 jsonSizeBytes = 0;
    };

    void setupUi();
    void refreshConfig();
    void startCapture(const QString& level, double temperature);
    void handleCaptureStatus(const nlohmann::json& data);
    void stopCapturePolling(bool clearTask);
    void setStatus(const QString& text, bool isError = false);
    void setResult(const QString& text, bool isError = false);
    void updateUiEnabled();
    void applyConfig(const nlohmann::json& data);
    void applyCaptureList(const nlohmann::json& data);
    void fillCaptureTable(QTableWidget* table, const QVector<CaptureItem>& items);
    CaptureItem selectedLow() const;
    CaptureItem selectedHigh() const;
    bool selectedPairValid(QString* error = nullptr) const;
    QString rpcErrorText(int code, const QString& message) const;

    DeviceClient* device_ = nullptr;
    bool connected_ = false;
    bool captureBusy_ = false;
    bool captureStatusPending_ = false;
    bool configBusy_ = false;
    bool calibrating_ = false;
    QString activeTaskId_;
    QString activeLevel_;

    bool currentEnabled_ = false;
    bool currentMatricesLoaded_ = false;
    QString currentGainFile_;
    QString currentOffsetFile_;
    double currentEps_ = 1e-6;
    int currentWidth_ = 0;
    int currentHeight_ = 0;

    QVector<CaptureItem> lowItems_;
    QVector<CaptureItem> highItems_;

    QCheckBox* enabledCheck_ = nullptr;
    QLabel* matricesLabel_ = nullptr;
    QLabel* sizeLabel_ = nullptr;
    QLabel* epsLabel_ = nullptr;
    QLabel* gainLabel_ = nullptr;
    QLabel* offsetLabel_ = nullptr;
    QPushButton* applyEnabledBtn_ = nullptr;
    QPushButton* reloadBtn_ = nullptr;
    QPushButton* refreshBtn_ = nullptr;

    QSpinBox* frameCountSpin_ = nullptr;
    QSpinBox* timeoutSpin_ = nullptr;
    QDoubleSpinBox* lowTemperatureSpin_ = nullptr;
    QDoubleSpinBox* highTemperatureSpin_ = nullptr;
    QPushButton* captureLowBtn_ = nullptr;
    QPushButton* captureHighBtn_ = nullptr;
    QPushButton* cancelBtn_ = nullptr;
    QLabel* captureStatusLabel_ = nullptr;

    QTableWidget* lowTable_ = nullptr;
    QTableWidget* highTable_ = nullptr;
    QLabel* lowSelectionLabel_ = nullptr;
    QLabel* highSelectionLabel_ = nullptr;
    QPushButton* calibrateBtn_ = nullptr;
    QLabel* resultLabel_ = nullptr;

    QTimer* captureTimer_ = nullptr;
};

#pragma once

#include "Client/core/DeviceTypes.h"
#include "Client/rpc/Protocol.h"
#include "Client/stream/StreamFrame.h"

#include <QObject>
#include <QSize>
#include <QTimer>
#include <array>

class BinningCompareWidget;
class BinningTestPanel;
class DeviceClient;

class BinningTestController : public QObject {
    Q_OBJECT
public:
    BinningTestController(DeviceClient* device,
                          BinningTestPanel* panel,
                          BinningCompareWidget* compare,
                          QObject* parent = nullptr);

    void refreshConfig();
    bool prepareForClose();
    QString closeBlockReason() const;

private:
    static BinningConfig symmetricConfig(int factor);
    static bool configMatches(const BinningConfig& config, int factor);
    static int measurementIndex(int factor);

    void startTest();
    void cancelTest();
    void applyManualFactor(int factor);
    void beginCurrentFactor(quint64 operation);
    void handleFrame(const StreamFrame& frame);
    void captureFrame(const StreamFrame& frame);
    void failTest(const QString& message);
    void finishAndRestore(const QString& completionText, bool error = false);
    void restoreOriginalConfig(const QString& completionText, bool error = false);
    void handleMeasurement(int factor, int pixels);
    void resetMeasurements(Qt::Orientation orientation);
    void updateMeasurementResults();

    DeviceClient* device_ = nullptr;
    BinningTestPanel* panel_ = nullptr;
    BinningCompareWidget* compare_ = nullptr;
    QTimer frameTimeout_;
    BinningConfig originalConfig_;
    bool originalConfigValid_ = false;
    bool restorePending_ = false;
    bool testRunning_ = false;
    bool restoring_ = false;
    bool awaitingFrame_ = false;
    int stepIndex_ = 0;
    int currentFactor_ = 1;
    int sourceChannel_ = cli::proto::Raw16;
    int settleFramesRemaining_ = 0;
    quint64 operation_ = 0;
    quint64 lastRawFrameId_ = 0;
    quint64 lastSliceFrameId_ = 0;
    quint64 frameFence_ = 0;
    QSize baselineSize_;
    std::array<int, 3> measurements_ = {{-1, -1, -1}};
};

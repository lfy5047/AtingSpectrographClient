#pragma once

#include "Client/core/DeviceTypes.h"
#include "Client/stream/StreamFrame.h"

#include <QObject>
#include <QSize>
#include <QTimer>

class DeviceClient;
class RoiCompareWidget;
class RoiTestPanel;

class RoiTestController : public QObject {
public:
    RoiTestController(DeviceClient* device,
                      RoiTestPanel* panel,
                      RoiCompareWidget* compare,
                      QObject* parent = nullptr);

    bool prepareForClose();
    QString closeBlockReason() const;

private:
    enum SnapshotStage {
        NoSnapshot,
        FullFrameSnapshot,
        TargetRoiSnapshot,
    };

    static bool roiMatches(const RoiConfig& lhs, const RoiConfig& rhs);
    static bool gateMatches(const CollectGateConfig& lhs, const CollectGateConfig& rhs);
    static QSize roiSize(const RoiConfig& config);
    static QString errorText(const QString& prefix, const QString& detail);

    bool validateRoi(const RoiConfig& config, const QSize& resolution, QString* error) const;
    void startTest();
    void cancelTest();
    void readOriginalRoi(quint64 operation);
    void readOriginalGate(quint64 operation);
    void enableStaticMode(quint64 operation);
    void configureFullFrame(quint64 operation);
    void startStaticCollection(quint64 operation);
    void configureTargetRoi(quint64 operation);
    void waitForSnapshot(SnapshotStage stage, const RoiConfig& config);
    void handleFrame(const StreamFrame& frame);
    void captureFrame(const StreamFrame& frame);
    void failTest(const QString& message);

    void beginRestore(const QString& completionText, bool error = false);
    void retryPendingRestore();
    void stopCollectionForRestore(quint64 operation, bool queryFirst);
    void restoreOriginalRoi(quint64 operation);
    void restoreOriginalGate(quint64 operation);
    void finishRestore(quint64 operation);
    void markRestorePending(const QString& message);
    void handleDisconnected();

    DeviceClient* device_ = nullptr;
    RoiTestPanel* panel_ = nullptr;
    RoiCompareWidget* compare_ = nullptr;
    QTimer frameTimeout_;
    QSize resolution_;
    RoiConfig targetConfig_;
    RoiConfig fullFrameConfig_;
    RoiConfig originalRoi_;
    CollectGateConfig originalGate_;
    bool originalRoiValid_ = false;
    bool originalGateValid_ = false;
    bool roiChanged_ = false;
    bool gateChanged_ = false;
    bool collectionStarted_ = false;
    bool testRunning_ = false;
    bool restoring_ = false;
    bool restorePending_ = false;
    bool applyWindowing_ = true;
    bool awaitingFrame_ = false;
    SnapshotStage snapshotStage_ = NoSnapshot;
    RoiConfig awaitedConfig_;
    quint64 operation_ = 0;
    quint64 lastSliceFrameId_ = 0;
    quint64 frameFence_ = 0;
    QString completionText_;
    bool completionError_ = false;
};

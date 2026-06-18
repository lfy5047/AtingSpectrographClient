#pragma once

#include <QObject>
#include <QString>
#include "ControlClient.h"
#include "StreamClient.h"
#include "StreamFrame.h"
#include "SystemService.h"
#include "MirrorService.h"
#include "CameraService.h"
#include "IrService.h"
#include "CollectService.h"
#include "TempControlService.h"
#include "StreamControlService.h"
#include "RecordService.h"

class DeviceClient : public QObject {
    Q_OBJECT
public:
    explicit DeviceClient(QObject* parent = nullptr);
    ~DeviceClient() override;

    ControlClient* control() { return &ctrl_; }
    StreamClient* stream() { return &stream_; }
    SystemService* systemApi() { return &system_; }
    MirrorService* mirror() { return &mirror_; }
    CameraService* camera() { return &camera_; }
    IrService* ir() { return &ir_; }
    CollectService* collect() { return &collect_; }
    TempControlService* tempControl() { return &tempControl_; }
    StreamControlService* streamControl() { return &streamControl_; }
    RecordService* record() { return &record_; }

    void connectTo(const QString& host, quint16 port);
    void disconnect();
    bool isConnected() const { return ctrl_.isConnected(); }

signals:
    void connectionChanged(bool connected, QString peerIp);
    void mirrorAngleEvent(double angle, bool isMoving, qint64 ts);
    void frameReady(StreamFrame frame);

private:
    ControlClient ctrl_;
    StreamClient stream_;
    SystemService system_;
    MirrorService mirror_;
    CameraService camera_;
    IrService ir_;
    CollectService collect_;
    TempControlService tempControl_;
    StreamControlService streamControl_;
    RecordService record_;
};

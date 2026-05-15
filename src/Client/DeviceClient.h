#pragma once

#include <QObject>
#include <QImage>
#include <functional>
#include "ControlClient.h"
#include "StreamClient.h"
#include "Protocol.h"

class DeviceClient : public QObject {
    Q_OBJECT
public:
    using Callback  = std::function<void(bool ok, const QString& err)>;
    using JsonCallback = std::function<void(bool ok, const nlohmann::json& data, const QString& err)>;

    explicit DeviceClient(QObject* parent = nullptr);
    ~DeviceClient() override;

    ControlClient* control() { return &ctrl_; }
    StreamClient*  stream() { return &stream_; }

    // ── connection ──
    void connectTo(const QString& host, quint16 port);
    void disconnect();
    bool isConnected() const { return ctrl_.isConnected(); }

    // ── system ──
    void ping(std::function<void(bool ok, qint64 ts, const QString& err)> cb);
    void version(std::function<void(bool ok, int ver, const QString& name, const QString& err)> cb);
    void status(JsonCallback cb);

    // ── stream subscribe ──
    void streamSubscribe(quint16 udpPort, const QStringList& channels, Callback cb);
    void streamUnsubscribeAll(Callback cb);
    void streamStatus(JsonCallback cb);

    // ── mirror ──
    void mirrorQueryAngle(std::function<void(bool ok, double angle, bool moving, const QString& err)> cb);
    void mirrorSetSpeed(int s, int f, Callback cb);
    void mirrorSetTarget(double angle, Callback cb);
    void mirrorSetTargetAbsolute(double angle, Callback cb);
    void mirrorStartMove(Callback cb);
    void mirrorStopMove(Callback cb);
    void mirrorHome(Callback cb);
    void mirrorSetHome(Callback cb);
    void mirrorGotoPreset(int id, Callback cb);

    // ── camera ──
    void cameraStartStream(Callback cb);
    void cameraStopStream(Callback cb);
    void cameraGetResolution(std::function<void(bool ok, int w, int h, const QString& err)> cb);
    void cameraSetResolution(int w, int h, Callback cb);

    // ── ir ──
    void irSendRaw(quint8 cmd, const QByteArray& data, quint8 readbackLen, JsonCallback cb);
    void irTriggerCalibration(Callback cb);
    void irSetBrightness(quint8 v, Callback cb);
    void irSetContrast(quint8 v, Callback cb);
    void irSetIntegration(quint16 v, Callback cb);
    void irReadModuleId(JsonCallback cb);
    void irReadSelfCheck(JsonCallback cb);
    void irReadFocusPlaneTemp(JsonCallback cb);
    void irReadCoreTemp(JsonCallback cb);
    void irQueryIntegrationTime(JsonCallback cb);

    // ── collect ──
    void collectStart(Callback cb);
    void collectStop(Callback cb);
    void collectStatus(std::function<void(bool ok, bool collecting, const QString& err)> cb);

signals:
    void connectionChanged(bool connected, QString peerIp);
    void mirrorAngleEvent(double angle, bool isMoving, qint64 ts);
    void frameReady(int channel, int width, int height, int pixfmt, QByteArray data);

private:
    void simpleCmd(const QString& cmd, const nlohmann::json& params, Callback cb, int timeout = 3000);

    ControlClient ctrl_;
    StreamClient  stream_;
};

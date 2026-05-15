#include "DeviceClient.h"

DeviceClient::DeviceClient(QObject* parent)
    : QObject(parent)
{
    connect(&ctrl_, &ControlClient::connectionChanged,
            this, &DeviceClient::connectionChanged);

    connect(&ctrl_, &ControlClient::eventReceived,
            this, [this](const QString& evt, const nlohmann::json& data) {
        if (evt == "mirror.angle") {
            double angle = data.value("angle", 0.0);
            bool moving  = data.value("is_moving", false);
            qint64 ts    = data.value("ts", (int64_t)0);
            emit mirrorAngleEvent(angle, moving, ts);
        }
    });

    connect(&stream_, &StreamClient::frameReady,
            this, &DeviceClient::frameReady);
}

DeviceClient::~DeviceClient() = default;

void DeviceClient::connectTo(const QString& host, quint16 port)
{
    ctrl_.connectTo(host, port);
}

void DeviceClient::disconnect()
{
    ctrl_.disconnectFromHost();
}

// ── helpers ──

void DeviceClient::simpleCmd(const QString& cmd, const nlohmann::json& params,
                              Callback cb, int timeout)
{
    ctrl_.request(cmd, params, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.msg);
    }, timeout);
}

// ── system ──

void DeviceClient::ping(std::function<void(bool, qint64, const QString&)> cb)
{
    ctrl_.request("system.ping", {}, [cb](const RpcResult& r) {
        qint64 ts = 0;
        if (r.ok) ts = r.data.value("ts", (int64_t)0);
        if (cb) cb(r.ok, ts, r.msg);
    });
}

void DeviceClient::version(std::function<void(bool, int, const QString&, const QString&)> cb)
{
    ctrl_.request("system.version", {}, [cb](const RpcResult& r) {
        int ver = 0;
        QString name;
        if (r.ok) {
            ver  = r.data.value("version", 0);
            name = QString::fromStdString(r.data.value("name", std::string()));
        }
        if (cb) cb(r.ok, ver, name, r.msg);
    });
}

void DeviceClient::status(JsonCallback cb)
{
    ctrl_.request("system.status", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

// ── stream ──

void DeviceClient::streamSubscribe(quint16 udpPort, const QStringList& channels, Callback cb)
{
    nlohmann::json chs = nlohmann::json::array();
    for (auto& c : channels) chs.push_back(c.toStdString());
    nlohmann::json p = {{"port", udpPort}, {"channels", chs}};
    simpleCmd("stream.subscribe", p, cb);
}

void DeviceClient::streamUnsubscribeAll(Callback cb)
{
    simpleCmd("stream.unsubscribe", {{"all", true}}, cb);
}

void DeviceClient::streamStatus(JsonCallback cb)
{
    ctrl_.request("stream.status", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

// ── mirror ──

void DeviceClient::mirrorQueryAngle(std::function<void(bool, double, bool, const QString&)> cb)
{
    ctrl_.request("mirror.query_angle", {}, [cb](const RpcResult& r) {
        double angle = 0;
        bool moving = false;
        if (r.ok) {
            angle  = r.data.value("angle", 0.0);
            moving = r.data.value("is_moving", false);
        }
        if (cb) cb(r.ok, angle, moving, r.msg);
    }, 5000);
}

void DeviceClient::mirrorSetSpeed(int s, int f, Callback cb)
{
    simpleCmd("mirror.set_speed", {{"s_speed", s}, {"f_speed", f}}, cb, 5000);
}

void DeviceClient::mirrorSetTarget(double angle, Callback cb)
{
    simpleCmd("mirror.set_target", {{"angle", angle}}, cb, 5000);
}

void DeviceClient::mirrorSetTargetAbsolute(double angle, Callback cb)
{
    simpleCmd("mirror.set_target_absolute", {{"angle", angle}}, cb, 5000);
}

void DeviceClient::mirrorStartMove(Callback cb)
{
    simpleCmd("mirror.start_move", {}, cb, 5000);
}

void DeviceClient::mirrorStopMove(Callback cb)
{
    simpleCmd("mirror.stop_move", {}, cb, 5000);
}

void DeviceClient::mirrorHome(Callback cb)
{
    simpleCmd("mirror.home", {}, cb, 5000);
}

void DeviceClient::mirrorSetHome(Callback cb)
{
    simpleCmd("mirror.set_home", {}, cb, 5000);
}

void DeviceClient::mirrorGotoPreset(int id, Callback cb)
{
    simpleCmd("mirror.goto_preset", {{"id", id}}, cb, 5000);
}

// ── camera ──

void DeviceClient::cameraStartStream(Callback cb)
{
    simpleCmd("camera.start_stream", {}, cb);
}

void DeviceClient::cameraStopStream(Callback cb)
{
    simpleCmd("camera.stop_stream", {}, cb);
}

void DeviceClient::cameraGetResolution(std::function<void(bool, int, int, const QString&)> cb)
{
    ctrl_.request("camera.get_resolution", {}, [cb](const RpcResult& r) {
        int w = 0, h = 0;
        if (r.ok) {
            w = r.data.value("width", 0);
            h = r.data.value("height", 0);
        }
        if (cb) cb(r.ok, w, h, r.msg);
    });
}

void DeviceClient::cameraSetResolution(int w, int h, Callback cb)
{
    simpleCmd("camera.set_resolution", {{"width", w}, {"height", h}}, cb);
}

// ── ir ──

void DeviceClient::irSendRaw(quint8 cmd, const QByteArray& data, quint8 readbackLen, JsonCallback cb)
{
    std::vector<uint8_t> d(data.begin(), data.end());
    nlohmann::json p = {{"cmd", cmd}, {"data", d}, {"readback_len", readbackLen}};
    ctrl_.request("ir.send_raw", p, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, 5000);
}

void DeviceClient::irTriggerCalibration(Callback cb)
{
    simpleCmd("ir.trigger_calibration", {}, cb, 5000);
}

void DeviceClient::irSetBrightness(quint8 v, Callback cb)
{
    simpleCmd("ir.set_brightness", {{"value", v}}, cb);
}

void DeviceClient::irSetContrast(quint8 v, Callback cb)
{
    simpleCmd("ir.set_contrast", {{"value", v}}, cb);
}

void DeviceClient::irSetIntegration(quint16 v, Callback cb)
{
    simpleCmd("ir.set_integration", {{"value", v}}, cb);
}

void DeviceClient::irReadModuleId(JsonCallback cb)
{
    ctrl_.request("ir.read_module_id", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, 5000);
}

void DeviceClient::irReadSelfCheck(JsonCallback cb)
{
    ctrl_.request("ir.read_self_check", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, 5000);
}

void DeviceClient::irReadFocusPlaneTemp(JsonCallback cb)
{
    ctrl_.request("ir.read_focus_plane_temp", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, 5000);
}

void DeviceClient::irReadCoreTemp(JsonCallback cb)
{
    ctrl_.request("ir.read_core_temp", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, 5000);
}

void DeviceClient::irQueryIntegrationTime(JsonCallback cb)
{
    ctrl_.request("ir.query_integration_time", {}, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, 5000);
}

// ── collect ──

void DeviceClient::collectStart(Callback cb)
{
    simpleCmd("collect.start", {}, cb);
}

void DeviceClient::collectStop(Callback cb)
{
    simpleCmd("collect.stop", {}, cb);
}

void DeviceClient::collectStatus(std::function<void(bool, bool, const QString&)> cb)
{
    ctrl_.request("collect.status", {}, [cb](const RpcResult& r) {
        bool collecting = false;
        if (r.ok) collecting = r.data.value("is_collecting", false);
        if (cb) cb(r.ok, collecting, r.msg);
    });
}

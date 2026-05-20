#include "DeviceClient.h"

DeviceClient::DeviceClient(QObject* parent)
    : QObject(parent)
    , ctrl_()
    , stream_()
    , system_(&ctrl_)
    , mirror_(&ctrl_)
    , camera_(&ctrl_)
    , ir_(&ctrl_)
    , collect_(&ctrl_)
    , streamControl_(&ctrl_)
{
    connect(&ctrl_, &ControlClient::connectionChanged,
            this, &DeviceClient::connectionChanged);

    connect(&ctrl_, &ControlClient::eventReceived,
            this, [this](const QString& evt, const nlohmann::json& data) {
        if (evt == "mirror.angle") {
            double angle = data.value("angle", 0.0);
            bool moving = data.value("is_moving", false);
            qint64 ts = data.value("ts", (int64_t)0);
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

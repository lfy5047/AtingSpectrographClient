#include "CameraService.h"

void CameraService::startStream(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Camera::StartStream, {}, context, cb);
}

void CameraService::stopStream(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Camera::StopStream, {}, context, cb);
}

void CameraService::getResolution(QObject* context, ResolutionCallback cb) const
{
    request(RpcCommand::Camera::GetResolution, {}, context, [cb](const RpcResult& r) {
        int w = 0;
        int h = 0;
        if (r.ok) {
            w = r.data.value("width", 0);
            h = r.data.value("height", 0);
        }
        if (cb) cb(r.ok, w, h, r.msg);
    });
}

void CameraService::setResolution(QObject* context, int w, int h, Callback cb) const
{
    simpleCmd(RpcCommand::Camera::SetResolution, {{"width", w}, {"height", h}}, context, cb);
}

void CameraService::selectDevice(QObject* context, const QString& mac, Callback cb) const
{
    simpleCmd(RpcCommand::Camera::SelectDevice, {{"mac", mac.toStdString()}}, context, cb);
}

void CameraService::clearSelectedDevice(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Camera::ClearSelectedDevice, {}, context, cb);
}

void CameraService::getSelectedDevice(QObject* context, CameraSelectedDeviceCallback cb) const
{
    request(RpcCommand::Camera::GetSelectedDevice, {}, context, [cb](const RpcResult& r) {
        QString mac;
        if (r.ok) mac = QString::fromStdString(r.data.value("mac", std::string()));
        if (cb) cb(r.ok, mac, r.msg);
    });
}

void CameraService::deviceOptions(QObject* context, CameraDeviceOptionsCallback cb) const
{
    request(RpcCommand::Camera::DeviceOptions, {}, context, [cb](const RpcResult& r) {
        std::vector<CameraDeviceOption> options;
        if (r.ok) {
            const auto it = r.data.find("options");
            if (it != r.data.end() && it->is_array()) {
                for (const auto& item : *it) {
                    if (!item.is_object()) continue;
                    CameraDeviceOption opt;
                    opt.name = QString::fromStdString(item.value("name", std::string()));
                    opt.mac  = QString::fromStdString(item.value("mac", std::string()));
                    if (opt.mac.isEmpty()) continue;
                    options.push_back(opt);
                }
            }
        }
        if (cb) cb(r.ok, options, r.msg);
    });
}

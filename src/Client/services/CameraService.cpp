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

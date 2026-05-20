#pragma once

#include "RpcServiceBase.h"

class CameraService : public RpcServiceBase {
public:
    explicit CameraService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void startStream(QObject* context, Callback cb) const;
    void stopStream(QObject* context, Callback cb) const;
    void getResolution(QObject* context, ResolutionCallback cb) const;
    void setResolution(QObject* context, int w, int h, Callback cb) const;
};

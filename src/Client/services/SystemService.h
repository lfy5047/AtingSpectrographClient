#pragma once

#include "RpcServiceBase.h"

class SystemService : public RpcServiceBase {
public:
    explicit SystemService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void ping(QObject* context, PingCallback cb) const;
    void version(QObject* context, VersionCallback cb) const;
    void status(QObject* context, JsonCallback cb) const;
};

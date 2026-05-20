#pragma once

#include "RpcServiceBase.h"

class CollectService : public RpcServiceBase {
public:
    explicit CollectService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void start(QObject* context, Callback cb) const;
    void stop(QObject* context, Callback cb) const;
    void status(QObject* context, CollectStatusCallback cb) const;
};

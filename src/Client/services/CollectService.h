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
    void getOversampling(QObject* context, CollectOversamplingCallback cb) const;
    void setOversampling(QObject* context, int oversampleFactor, CollectOversamplingCallback cb) const;
    void getGateConfig(QObject* context, CollectGateConfigCallback cb) const;
    void setGateConfig(QObject* context, const CollectGateConfig& config, CollectGateConfigCallback cb) const;
};

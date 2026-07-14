#pragma once

#include "RpcServiceBase.h"

class BinningService : public RpcServiceBase {
public:
    explicit BinningService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void getConfig(QObject* context, BinningConfigCallback cb) const;
    void setConfig(QObject* context, const BinningConfig& config, BinningConfigCallback cb) const;
};

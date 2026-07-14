#pragma once

#include "RpcServiceBase.h"

class RoiService : public RpcServiceBase {
public:
    explicit RoiService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void getConfig(QObject* context, RoiConfigCallback cb) const;
    void setConfig(QObject* context, const RoiConfig& config, RoiConfigCallback cb) const;
};

#pragma once

#include <QPointer>
#include <QObject>
#include "ControlClient.h"
#include "DeviceTypes.h"
#include "RpcCommands.h"

class RpcServiceBase {
protected:
    explicit RpcServiceBase(ControlClient* ctrl)
        : ctrl_(ctrl)
    {
    }

    void request(const QString& cmd,
                 const nlohmann::json& params,
                 QObject* context,
                 ControlClient::ResponseHandler cb,
                 int timeout_ms = RpcTimeout::Normal) const
    {
        const bool hasContext = (context != nullptr);
        QPointer<QObject> guard(context);

        ctrl_->request(cmd, params, [hasContext, guard, cb](const RpcResult& r) {
            if (hasContext && guard.isNull()) return;
            if (cb) cb(r);
        }, timeout_ms);
    }

    void simpleCmd(const QString& cmd,
                   const nlohmann::json& params,
                   QObject* context,
                   Callback cb,
                   int timeout_ms = RpcTimeout::Normal) const
    {
        request(cmd, params, context, [cb](const RpcResult& r) {
            if (cb) cb(r.ok, r.msg);
        }, timeout_ms);
    }

    ControlClient* ctrl_ = nullptr;
};

#pragma once

#include <QStringList>
#include "RpcServiceBase.h"

class StreamControlService : public RpcServiceBase {
public:
    explicit StreamControlService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void subscribe(QObject* context, quint16 udpPort, const QStringList& channels, Callback cb) const;
    void unsubscribeAll(QObject* context, Callback cb) const;
    void status(QObject* context, JsonCallback cb) const;
};

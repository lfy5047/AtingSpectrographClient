#pragma once

#include <QByteArray>
#include "RpcServiceBase.h"

class IrService : public RpcServiceBase {
public:
    explicit IrService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void sendRaw(QObject* context, quint8 cmd, const QByteArray& data, quint8 readbackLen, JsonCallback cb) const;
    void triggerCalibration(QObject* context, Callback cb) const;
    void setBrightness(QObject* context, quint8 v, Callback cb) const;
    void setContrast(QObject* context, quint8 v, Callback cb) const;
    void setIntegration(QObject* context, quint16 v, Callback cb) const;
    void readModuleId(QObject* context, JsonCallback cb) const;
    void readSelfCheck(QObject* context, JsonCallback cb) const;
    void readFocusPlaneTemp(QObject* context, JsonCallback cb) const;
    void readCoreTemp(QObject* context, JsonCallback cb) const;
    void queryIntegrationTime(QObject* context, JsonCallback cb) const;
};

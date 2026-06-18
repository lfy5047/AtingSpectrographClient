#pragma once

#include "RpcServiceBase.h"

struct TempControlStatus {
    double adjustTemperature = 0.0;
    double actualTemperature = 0.0;
    bool switchEnabled = false;
    bool outputEnabled = false;
    QString errorStatus;
    qint64 timestamp = 0;
};

using TempControlStatusCallback =
    std::function<void(bool ok, const TempControlStatus& status, const QString& err)>;

class TempControlService : public RpcServiceBase {
public:
    explicit TempControlService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void params(QObject* context, JsonCallback cb) const;
    void status(QObject* context, TempControlStatusCallback cb) const;
    void setAdjustTemperature(QObject* context, double value, JsonCallback cb) const;
    void setSwitch(QObject* context, bool enable, JsonCallback cb) const;
    void query(QObject* context, const QString& key, JsonCallback cb) const;
    void queryRawParam(QObject* context, const QString& module, const QString& param, JsonCallback cb) const;
    void set(QObject* context, const QString& key, const QString& value, JsonCallback cb) const;
    void setRawParam(QObject* context, const QString& module, const QString& param,
                     const QString& value, JsonCallback cb) const;
    void save(QObject* context, const QString& key, JsonCallback cb) const;
    void saveRawParam(QObject* context, const QString& module, const QString& param, JsonCallback cb) const;
    void sendRaw(QObject* context, const QString& command, JsonCallback cb) const;
};

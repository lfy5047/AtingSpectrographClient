#include "SystemService.h"
#include <string>

void SystemService::ping(QObject* context, PingCallback cb) const
{
    request(RpcCommand::System::Ping, {}, context, [cb](const RpcResult& r) {
        qint64 ts = 0;
        if (r.ok) ts = r.data.value("ts", static_cast<qint64>(0));
        if (cb) cb(r.ok, ts, r.msg);
    });
}

void SystemService::version(QObject* context, VersionCallback cb) const
{
    request(RpcCommand::System::Version, {}, context, [cb](const RpcResult& r) {
        int ver = 0;
        QString name;
        if (r.ok) {
            ver = r.data.value("version", 0);
            name = QString::fromStdString(r.data.value("name", std::string()));
        }
        if (cb) cb(r.ok, ver, name, r.msg);
    });
}

void SystemService::status(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::System::Status, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

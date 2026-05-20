#include "StreamControlService.h"

void StreamControlService::subscribe(QObject* context, quint16 udpPort, const QStringList& channels, Callback cb) const
{
    nlohmann::json chs = nlohmann::json::array();
    for (const auto& c : channels) chs.push_back(c.toStdString());

    nlohmann::json params = {
        {"port", udpPort},
        {"channels", chs},
    };
    simpleCmd(RpcCommand::StreamControl::Subscribe, params, context, cb);
}

void StreamControlService::unsubscribeAll(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::StreamControl::UnsubscribeAll, {{"all", true}}, context, cb);
}

void StreamControlService::status(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::StreamControl::Status, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

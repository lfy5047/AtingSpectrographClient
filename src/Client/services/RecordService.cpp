#include "RecordService.h"

void RecordService::getRetention(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Record::GetRetention, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

void RecordService::setRetention(QObject* context, quint64 seconds, JsonCallback cb) const
{
    request(RpcCommand::Record::SetRetention, {{"seconds", seconds}}, context,
            [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

void RecordService::listRecent(QObject* context, const QString& type, quint64 count,
                               quint64 lastSeconds, JsonCallback cb) const
{
    nlohmann::json params = {{"type", type.toStdString()}};
    if (count > 0) params["count"] = count;
    if (lastSeconds > 0) params["last_seconds"] = lastSeconds;
    request(RpcCommand::Record::ListRecent, params, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

void RecordService::fetch(QObject* context, const QString& type, const QStringList& recordIds,
                          JsonCallback cb) const
{
    nlohmann::json items = nlohmann::json::array();
    for (const QString& id : recordIds) {
        items.push_back(id.toStdString());
    }
    request(RpcCommand::Record::Fetch, {{"type", type.toStdString()}, {"items", items}},
            context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void RecordService::removeRecords(QObject* context, const QString& type, const QStringList& recordIds,
                                  JsonCallback cb) const
{
    nlohmann::json items = nlohmann::json::array();
    for (const QString& id : recordIds) {
        items.push_back(id.toStdString());
    }
    request(RpcCommand::Record::Delete, {{"type", type.toStdString()}, {"items", items}},
            context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

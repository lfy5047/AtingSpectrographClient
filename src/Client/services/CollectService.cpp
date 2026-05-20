#include "CollectService.h"

void CollectService::start(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Collect::Start, {}, context, cb);
}

void CollectService::stop(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Collect::Stop, {}, context, cb);
}

void CollectService::status(QObject* context, CollectStatusCallback cb) const
{
    request(RpcCommand::Collect::Status, {}, context, [cb](const RpcResult& r) {
        bool collecting = false;
        if (r.ok) collecting = r.data.value("is_collecting", false);
        if (cb) cb(r.ok, collecting, r.msg);
    });
}

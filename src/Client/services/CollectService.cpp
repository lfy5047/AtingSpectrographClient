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

namespace {

CollectOversamplingInfo oversamplingInfoFromJson(const nlohmann::json& data)
{
    CollectOversamplingInfo info;
    info.oversampleFactor = data.value("oversample_factor", 1);
    info.effectiveSSpeed = data.value("effective_s_speed", 0);
    info.effectiveFSpeed = data.value("effective_f_speed", 0);
    info.collecting = data.value("is_collecting", false);
    return info;
}

}

void CollectService::getOversampling(QObject* context, CollectOversamplingCallback cb) const
{
    request(RpcCommand::Collect::GetOversampling, {}, context, [cb](const RpcResult& r) {
        const CollectOversamplingInfo info = r.ok ? oversamplingInfoFromJson(r.data)
                                                  : CollectOversamplingInfo();
        if (cb) cb(r.ok, info, r.msg);
    });
}

void CollectService::setOversampling(QObject* context, int oversampleFactor,
                                     CollectOversamplingCallback cb) const
{
    request(RpcCommand::Collect::SetOversampling, {{"oversample_factor", oversampleFactor}},
            context, [cb](const RpcResult& r) {
        const CollectOversamplingInfo info = r.ok ? oversamplingInfoFromJson(r.data)
                                                  : CollectOversamplingInfo();
        if (cb) cb(r.ok, info, r.msg);
    });
}

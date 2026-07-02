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

CollectGateConfig gateConfigFromJson(const nlohmann::json& data)
{
    CollectGateConfig config;
    config.discardFrontMs = data.value("discard_front_ms", 0);
    config.discardBackMs = data.value("discard_back_ms", 0);
    config.forwardOffsetFrames = data.value("forward_offset_frames", 0);
    config.reverseOffsetFrames = data.value("reverse_offset_frames", 0);
    config.staticCollectMode = data.value("static_collect_mode", false);
    config.collecting = data.value("is_collecting", false);
    config.pendingConfig = data.value("pending_config", false);
    return config;
}

nlohmann::json gateConfigToJson(const CollectGateConfig& config)
{
    return {
        {"discard_front_ms", config.discardFrontMs},
        {"discard_back_ms", config.discardBackMs},
        {"forward_offset_frames", config.forwardOffsetFrames},
        {"reverse_offset_frames", config.reverseOffsetFrames},
        {"static_collect_mode", config.staticCollectMode},
    };
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

void CollectService::getGateConfig(QObject* context, CollectGateConfigCallback cb) const
{
    request(RpcCommand::Collect::GetGateConfig, {}, context, [cb](const RpcResult& r) {
        const CollectGateConfig config = r.ok ? gateConfigFromJson(r.data)
                                              : CollectGateConfig();
        if (cb) cb(r.ok, config, r.msg);
    });
}

void CollectService::setGateConfig(QObject* context, const CollectGateConfig& config,
                                   CollectGateConfigCallback cb) const
{
    request(RpcCommand::Collect::SetGateConfig, gateConfigToJson(config),
            context, [cb](const RpcResult& r) {
        const CollectGateConfig updatedConfig = r.ok ? gateConfigFromJson(r.data)
                                                     : CollectGateConfig();
        if (cb) cb(r.ok, updatedConfig, r.msg);
    });
}

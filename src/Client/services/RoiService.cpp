#include "RoiService.h"

namespace {

RoiConfig configFromJson(const nlohmann::json& data)
{
    RoiConfig config;
    config.sliceBegin = data.value("slice_begin", 0);
    config.sliceEnd = data.value("slice_end", 0);
    config.sliceHBegin = data.value("slice_h_begin", 0);
    config.sliceHEnd = data.value("slice_h_end", 0);
    config.collecting = data.value("is_collecting", false);
    config.pendingApply = data.value("pending_apply", false);
    return config;
}

nlohmann::json configToJson(const RoiConfig& config)
{
    return {
        {"slice_begin", config.sliceBegin},
        {"slice_end", config.sliceEnd},
        {"slice_h_begin", config.sliceHBegin},
        {"slice_h_end", config.sliceHEnd},
    };
}

} // namespace

void RoiService::getConfig(QObject* context, RoiConfigCallback cb) const
{
    request(RpcCommand::Roi::GetConfig, {}, context, [cb](const RpcResult& result) {
        const RoiConfig config = result.ok ? configFromJson(result.data) : RoiConfig();
        if (cb) cb(result.ok, config, result.msg);
    });
}

void RoiService::setConfig(QObject* context, const RoiConfig& config,
                           RoiConfigCallback cb) const
{
    request(RpcCommand::Roi::SetConfig, configToJson(config), context,
            [cb](const RpcResult& result) {
        const RoiConfig updated = result.ok ? configFromJson(result.data) : RoiConfig();
        if (cb) cb(result.ok, updated, result.msg);
    });
}

#include "BinningService.h"

namespace {

BinningConfig configFromJson(const nlohmann::json& data)
{
    BinningConfig config;
    config.enabled = data.value("enabled", false);
    config.spectralFactor = data.value("spectral_factor", 1);
    config.spatialFactor = data.value("spatial_factor", 1);
    return config;
}

nlohmann::json configToJson(const BinningConfig& config)
{
    return {
        {"enabled", config.enabled},
        {"spectral_factor", config.spectralFactor},
        {"spatial_factor", config.spatialFactor},
    };
}

} // namespace

void BinningService::getConfig(QObject* context, BinningConfigCallback cb) const
{
    request(RpcCommand::Binning::GetConfig, {}, context, [cb](const RpcResult& result) {
        const BinningConfig config = result.ok ? configFromJson(result.data) : BinningConfig();
        if (cb) cb(result.ok, config, result.msg);
    });
}

void BinningService::setConfig(QObject* context, const BinningConfig& config,
                               BinningConfigCallback cb) const
{
    request(RpcCommand::Binning::SetConfig, configToJson(config), context,
            [cb](const RpcResult& result) {
        const BinningConfig updated = result.ok ? configFromJson(result.data) : BinningConfig();
        if (cb) cb(result.ok, updated, result.msg);
    });
}

#include "TempControlService.h"

namespace {

QString jsonValueToQString(const nlohmann::json& value)
{
    if (value.is_string()) {
        return QString::fromStdString(value.get<std::string>());
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? QStringLiteral("1") : QStringLiteral("0");
    }
    if (value.is_number_integer()) {
        return QString::number(value.get<qint64>());
    }
    if (value.is_number_unsigned()) {
        return QString::number(value.get<quint64>());
    }
    if (value.is_number_float()) {
        return QString::number(value.get<double>(), 'f', 3);
    }
    if (value.is_null()) {
        return QString();
    }
    return QString::fromStdString(value.dump());
}

bool jsonValueToBool(const nlohmann::json& data, const char* key)
{
    const auto it = data.find(key);
    if (it == data.end()) return false;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number_integer()) return it->get<int>() != 0;
    if (it->is_number_unsigned()) return it->get<unsigned int>() != 0;
    if (it->is_string()) {
        const QString text = QString::fromStdString(it->get<std::string>()).trimmed().toLower();
        return text == QStringLiteral("1") || text == QStringLiteral("true")
            || text == QStringLiteral("on") || text == QStringLiteral("yes");
    }
    return false;
}

TempControlStatus statusFromJson(const nlohmann::json& data)
{
    TempControlStatus status;
    status.adjustTemperature = data.value("adjust_temperature", 0.0);
    status.actualTemperature = data.value("actual_temperature", 0.0);
    status.actualVoltage = data.value("actual_voltage", 0.0);
    status.switchEnabled = jsonValueToBool(data, "switch");
    status.outputEnabled = jsonValueToBool(data, "output_enabled");
    const auto errorIt = data.find("error_status");
    if (errorIt != data.end()) {
        status.errorStatus = jsonValueToQString(*errorIt);
    }
    status.timestamp = data.value("ts", static_cast<qint64>(0));
    return status;
}

nlohmann::json keyParams(const QString& key)
{
    return {{"key", key.toStdString()}};
}

nlohmann::json rawParamParams(const QString& module, const QString& param)
{
    return {
        {"module", module.toStdString()},
        {"param", param.toStdString()},
    };
}

void returnJson(JsonCallback cb, const RpcResult& r)
{
    if (cb) cb(r.ok, r.data, r.msg);
}

}

void TempControlService::params(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::TempControl::Params, {}, context, [cb](const RpcResult& r) {
        returnJson(cb, r);
    }, RpcTimeout::Slow);
}

void TempControlService::status(QObject* context, TempControlStatusCallback cb) const
{
    request(RpcCommand::TempControl::Status, {}, context, [cb](const RpcResult& r) {
        const TempControlStatus status = r.ok ? statusFromJson(r.data) : TempControlStatus();
        if (cb) cb(r.ok, status, r.msg);
    }, RpcTimeout::Slow);
}

void TempControlService::setAdjustTemperature(QObject* context, double value, JsonCallback cb) const
{
    request(RpcCommand::TempControl::SetAdjustTemperature, {{"value", value}},
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::setSwitch(QObject* context, bool enable, JsonCallback cb) const
{
    request(RpcCommand::TempControl::SetSwitch, {{"enable", enable}},
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::query(QObject* context, const QString& key, JsonCallback cb) const
{
    request(RpcCommand::TempControl::Query, keyParams(key),
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::queryRawParam(QObject* context, const QString& module,
                                       const QString& param, JsonCallback cb) const
{
    request(RpcCommand::TempControl::Query, rawParamParams(module, param),
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::set(QObject* context, const QString& key, const QString& value,
                             JsonCallback cb) const
{
    nlohmann::json params = keyParams(key);
    params["value"] = value.toStdString();
    request(RpcCommand::TempControl::Set, params,
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::setRawParam(QObject* context, const QString& module,
                                     const QString& param, const QString& value,
                                     JsonCallback cb) const
{
    nlohmann::json params = rawParamParams(module, param);
    params["value"] = value.toStdString();
    request(RpcCommand::TempControl::Set, params,
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::save(QObject* context, const QString& key, JsonCallback cb) const
{
    request(RpcCommand::TempControl::Save, keyParams(key),
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::saveRawParam(QObject* context, const QString& module,
                                      const QString& param, JsonCallback cb) const
{
    request(RpcCommand::TempControl::Save, rawParamParams(module, param),
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

void TempControlService::sendRaw(QObject* context, const QString& command, JsonCallback cb) const
{
    request(RpcCommand::TempControl::SendRaw, {{"command", command.toStdString()}},
            context, [cb](const RpcResult& r) { returnJson(cb, r); }, RpcTimeout::Slow);
}

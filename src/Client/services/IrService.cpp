#include "IrService.h"
#include <vector>

void IrService::sendRaw(QObject* context, quint8 cmd, const QByteArray& data, quint8 readbackLen, JsonCallback cb) const
{
    std::vector<uint8_t> payloadData(data.begin(), data.end());
    nlohmann::json params = {
        {"cmd", cmd},
        {"data", payloadData},
        {"readback_len", readbackLen},
    };
    request(RpcCommand::Ir::SendRaw, params, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::triggerCalibration(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::TriggerCalibration, {}, context, cb, RpcTimeout::Slow);
}

void IrService::setBrightness(QObject* context, quint8 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetBrightness, {{"value", v}}, context, cb);
}

void IrService::setContrast(QObject* context, quint8 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetContrast, {{"value", v}}, context, cb);
}

void IrService::setIntegration(QObject* context, quint16 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetIntegration, {{"value", v}}, context, cb);
}

void IrService::readModuleId(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadModuleId, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::readSelfCheck(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadSelfCheck, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::readFocusPlaneTemp(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadFocusPlaneTemp, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::readCoreTemp(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadCoreTemp, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::queryIntegrationTime(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::QueryIntegrationTime, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

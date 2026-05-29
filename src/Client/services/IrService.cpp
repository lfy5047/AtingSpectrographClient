#include "IrService.h"
#include <vector>

// ---- 基础标定 ----

void IrService::triggerCalibration(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::TriggerCalibration, {}, context, cb, RpcTimeout::Slow);
}

void IrService::forceShutter(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::ForceShutter, {}, context, cb, RpcTimeout::Slow);
}

// ---- 版本 ----

void IrService::getVersion(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::GetVersion, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

// ---- 图像选择与显示 ----

void IrService::setImageType(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetImageType, {{"value", value}}, context, cb);
}

void IrService::setTestPattern(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetTestPattern, {{"value", value}}, context, cb);
}

void IrService::setColorMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetColorMode, {{"value", value}}, context, cb);
}

void IrService::setBadPixelDisplayMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetBadPixelDisplayMode, {{"value", value}}, context, cb);
}

// ---- 图像参数 ----

void IrService::setBrightness(QObject* context, quint8 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetBrightness, {{"value", v}}, context, cb);
}

void IrService::setContrast(QObject* context, quint8 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetContrast, {{"value", v}}, context, cb);
}

void IrService::setAbMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetAbMode, {{"value", value}}, context, cb);
}

void IrService::setDde(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetDde, {{"value", value}}, context, cb);
}

void IrService::setTemporalFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetTemporalFilter, {{"enable", enable}, {"coeff", coeff}}, context, cb);
}

void IrService::setMedianFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetMedianFilter, {{"enable", enable}, {"coeff", coeff}}, context, cb);
}

// ---- 翻转与同步 ----

void IrService::setFlipHorizontal(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetFlipHorizontal, {{"value", value}}, context, cb);
}

void IrService::setFlipVertical(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetFlipVertical, {{"value", value}}, context, cb);
}

void IrService::setExternalSync(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetExternalSync, {{"value", value}}, context, cb);
}

// ---- 积分时间 ----

void IrService::setIntegration(QObject* context, quint16 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetIntegration, {{"value", v}}, context, cb);
}

void IrService::setManualIntegration(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetManualIntegration, {{"value", value}}, context, cb);
}

void IrService::setIntegrationGearMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetIntegrationGearMode, {{"value", value}}, context, cb);
}

void IrService::selectIntegrationGear(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SelectIntegrationGear, {{"value", value}}, context, cb);
}

void IrService::queryIntegrationTime(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::QueryIntegrationTime, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

// ---- 模式控制 ----

void IrService::setStandby(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetStandby, {{"value", value}}, context, cb);
}

void IrService::setOnboardAutoCalibration(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SetOnboardAutoCalibration, {{"value", value}}, context, cb);
}

// ---- 读取查询 ----

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

void IrService::readMean(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadMean, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::readCorrectionParamGear(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadCorrectionParamGear, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::readCoreTemp(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadCoreTemp, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::readBadPixelCount(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::ReadBadPixelCount, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

// ---- 维护与校正 ----

void IrService::maintenanceUnlock(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::MaintenanceUnlock, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::maintenanceExec(QObject* context, const QString& name, quint8 value, JsonCallback cb) const
{
    request(RpcCommand::Ir::MaintenanceExec, {{"name", name.toStdString()}, {"value", value}}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::twoPointCalibP1(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::TwoPointCalibP1, {}, context, cb, RpcTimeout::Slow);
}

void IrService::twoPointCalibP2(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::TwoPointCalibP2, {}, context, cb, RpcTimeout::Slow);
}

void IrService::saveCalibParams(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::SaveCalibParams, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    }, RpcTimeout::Slow);
}

void IrService::clearK(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::ClearK, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::clearB(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::ClearB, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

// ---- 坏元管理 ----

void IrService::badPixelSearch(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::BadPixelSearch, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::setBadPixelPosition(QObject* context, const quint8 pos[4], Callback cb) const
{
    std::vector<uint8_t> arr = {pos[0], pos[1], pos[2], pos[3]};
    simpleCmd(RpcCommand::Ir::SetBadPixelPosition, {{"pos", arr}}, context, cb, RpcTimeout::Slow);
}

void IrService::saveBadPixel(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::SaveBadPixel, {}, context, cb, RpcTimeout::Slow);
}

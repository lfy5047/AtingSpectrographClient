#include "IrService.h"

#include <vector>

namespace {

void callJson(const JsonCallback& cb, const RpcResult& r)
{
    if (cb) cb(r.ok, r.data, r.msg);
}

} // namespace

void IrService::currentModel(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Core::Current, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::triggerCalibration(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::TriggerCalibration, {}, context, cb, RpcTimeout::Slow);
}

void IrService::forceShutter(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::ForceShutter, {}, context, cb, RpcTimeout::Slow);
}

void IrService::getVersion(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::GetVersion, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::setImageType(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetImageType, {{"value", value}}, context, cb);
}

void IrService::setTestPattern(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetTestPattern, {{"value", value}}, context, cb);
}

void IrService::setColorMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetColorMode, {{"value", value}}, context, cb);
}

void IrService::setBadPixelDisplayMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetBadPixelDisplayMode, {{"value", value}}, context, cb);
}

void IrService::setBrightness(QObject* context, quint8 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetBrightness, {{"value", v}}, context, cb);
}

void IrService::setContrast(QObject* context, quint8 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetContrast, {{"value", v}}, context, cb);
}

void IrService::setAbMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetAbMode, {{"value", value}}, context, cb);
}

void IrService::setDde(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetDde, {{"value", value}}, context, cb);
}

void IrService::setTemporalFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetTemporalFilter, {{"enable", enable}, {"coeff", coeff}}, context, cb);
}

void IrService::setMedianFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetMedianFilter, {{"enable", enable}, {"coeff", coeff}}, context, cb);
}

void IrService::setFlipHorizontal(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetFlipHorizontal, {{"value", value}}, context, cb);
}

void IrService::setFlipVertical(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetFlipVertical, {{"value", value}}, context, cb);
}

void IrService::setExternalSync(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetExternalSync, {{"value", value}}, context, cb);
}

void IrService::setIntegration(QObject* context, quint16 v, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetIntegration, {{"value", v}}, context, cb);
}

void IrService::setManualIntegration(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetManualIntegration, {{"value", value}}, context, cb);
}

void IrService::setIntegrationGearMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetIntegrationGearMode, {{"value", value}}, context, cb);
}

void IrService::selectIntegrationGear(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SelectIntegrationGear, {{"value", value}}, context, cb);
}

void IrService::queryIntegrationTime(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::QueryIntegrationTime, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::setStandby(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetStandby, {{"value", value}}, context, cb);
}

void IrService::setOnboardAutoCalibration(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SetOnboardAutoCalibration, {{"value", value}}, context, cb);
}

void IrService::readModuleId(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadModuleId, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::readSelfCheck(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadSelfCheck, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::readFocusPlaneTemp(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadFocusPlaneTemp, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::readMean(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadMean, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::readCorrectionParamGear(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadCorrectionParamGear, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::readCoreTemp(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadCoreTemp, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::readBadPixelCount(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::ReadBadPixelCount, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::maintenanceUnlock(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::MaintenanceUnlock, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::maintenanceExec(QObject* context, const QString& name, quint8 value, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::MaintenanceExec, {{"name", name.toStdString()}, {"value", value}},
            context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::twoPointCalibP1(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::TwoPointCalibP1, {}, context, cb, RpcTimeout::Slow);
}

void IrService::twoPointCalibP2(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::TwoPointCalibP2, {}, context, cb, RpcTimeout::Slow);
}

void IrService::saveCalibParams(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Legacy::SaveCalibParams, {}, context, [cb](const RpcResult& r) {
        callJson(cb, r);
    }, RpcTimeout::Slow);
}

void IrService::clearK(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::ClearK, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::clearB(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::ClearB, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::badPixelSearch(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::BadPixelSearch, {{"value", value}}, context, cb, RpcTimeout::Slow);
}

void IrService::setBadPixelPosition(QObject* context, const quint8 pos[4], Callback cb) const
{
    std::vector<uint8_t> arr = {pos[0], pos[1], pos[2], pos[3]};
    simpleCmd(RpcCommand::Ir::Legacy::SetBadPixelPosition, {{"pos", arr}}, context, cb, RpcTimeout::Slow);
}

void IrService::saveBadPixel(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Legacy::SaveBadPixel, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05FocusStartPositive(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::FocusStartPositive, {}, context, cb);
}

void IrService::ci05FocusStartNegative(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::FocusStartNegative, {}, context, cb);
}

void IrService::ci05FocusStop(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::FocusStop, {}, context, cb);
}

void IrService::ci05FocusStepPositive(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::FocusStepPositive, {}, context, cb);
}

void IrService::ci05FocusStepNegative(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::FocusStepNegative, {}, context, cb);
}

void IrService::ci05ZoomStartPositive(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ZoomStartPositive, {}, context, cb);
}

void IrService::ci05ZoomStartNegative(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ZoomStartNegative, {}, context, cb);
}

void IrService::ci05ZoomStop(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ZoomStop, {}, context, cb);
}

void IrService::ci05ZoomStepPositive(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ZoomStepPositive, {}, context, cb);
}

void IrService::ci05ZoomStepNegative(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ZoomStepNegative, {}, context, cb);
}

void IrService::ci05AutoFocus(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::AutoFocus, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05SetFov(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetFov, {{"value", value}}, context, cb);
}

void IrService::ci05ShutterOpen(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ShutterOpen, {}, context, cb);
}

void IrService::ci05ShutterClose(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::ShutterClose, {}, context, cb);
}

void IrService::ci05SetFocusSpeed(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetFocusSpeed, {{"value", value}}, context, cb);
}

void IrService::ci05SetZoomSpeed(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetZoomSpeed, {{"value", value}}, context, cb);
}

void IrService::ci05CallPreset(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::CallPreset, {{"value", value}}, context, cb);
}

void IrService::ci05SetPreset(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetPreset, {{"value", value}}, context, cb);
}

void IrService::ci05SetFocalLengthMmX10(QObject* context, quint16 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetFocalLengthMmX10, {{"value", value}}, context, cb);
}

void IrService::ci05QueryFocalLength(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::QueryFocalLength, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05QueryFocusMotorPosition(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::QueryFocusMotorPosition, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05QueryZoomMotorPosition(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::QueryZoomMotorPosition, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05MenuUser(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::MenuUser, {}, context, cb);
}

void IrService::ci05MenuRight(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::MenuRight, {}, context, cb);
}

void IrService::ci05MenuLeft(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::MenuLeft, {}, context, cb);
}

void IrService::ci05MenuParamInc(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::MenuParamInc, {}, context, cb);
}

void IrService::ci05MenuParamDec(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::MenuParamDec, {}, context, cb);
}

void IrService::ci05PromptOn(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::PromptOn, {}, context, cb);
}

void IrService::ci05PromptOff(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::PromptOff, {}, context, cb);
}

void IrService::ci05SetSyncMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetSyncMode, {{"value", value}}, context, cb);
}

void IrService::ci05SetBrightness(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetBrightness, {{"value", value}}, context, cb);
}

void IrService::ci05SetContrast(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetContrast, {{"value", value}}, context, cb);
}

void IrService::ci05SetOverallBrightness(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetOverallBrightness, {{"value", value}}, context, cb);
}

void IrService::ci05SetOverallContrast(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetOverallContrast, {{"value", value}}, context, cb);
}

void IrService::ci05SetSharpness(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetSharpness, {{"value", value}}, context, cb);
}

void IrService::ci05SetY8Level(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetY8Level, {{"value", value}}, context, cb);
}

void IrService::ci05SetEzoom(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetEzoom, {{"value", value}}, context, cb);
}

void IrService::ci05SetFreeze(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetFreeze, {{"value", value}}, context, cb);
}

void IrService::ci05SetMirrorMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetMirrorMode, {{"value", value}}, context, cb);
}

void IrService::ci05SetPolarityPalette(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetPolarityPalette, {{"value", value}}, context, cb);
}

void IrService::ci05SetAgcMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetAgcMode, {{"value", value}}, context, cb);
}

void IrService::ci05SaveParams(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SaveParams, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05SetIntegrationMsX10(QObject* context, quint16 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetIntegrationMsX10, {{"value", value}}, context, cb);
}

void IrService::ci05IntegrationIncrease0p1Ms(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::IntegrationIncrease0p1Ms, {}, context, cb);
}

void IrService::ci05IntegrationDecrease0p1Ms(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::IntegrationDecrease0p1Ms, {}, context, cb);
}

void IrService::ci05SetIntegrationMc(QObject* context, quint32 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetIntegrationMc, {{"value", value}}, context, cb);
}

void IrService::ci05SetFrameRateHzX100(QObject* context, quint16 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetFrameRateHzX100, {{"value", value}}, context, cb);
}

void IrService::ci05ReadFrameRateHz(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadFrameRateHz, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05SetIntegrationGear(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetIntegrationGear, {{"value", value}}, context, cb);
}

void IrService::ci05SetIntegrationGearAuto(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetIntegrationGearAuto, {{"value", value}}, context, cb);
}

void IrService::ci05SetBackgroundGear(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetBackgroundGear, {{"value", value}}, context, cb);
}

void IrService::ci05SetBackgroundGearAuto(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetBackgroundGearAuto, {{"value", value}}, context, cb);
}

void IrService::ci05TriggerShutterCompensation(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::TriggerShutterCompensation, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05TriggerSceneCompensation(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::TriggerSceneCompensation, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05TriggerDefocusCompensation(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::TriggerDefocusCompensation, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05TriggerIntegrationCorrection(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::TriggerIntegrationCorrection, {}, context, cb, RpcTimeout::Slow);
}

void IrService::ci05SetBootCompensationMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetBootCompensationMode, {{"value", value}}, context, cb);
}

void IrService::ci05SetGearSwitchCompensationMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetGearSwitchCompensationMode, {{"value", value}}, context, cb);
}

void IrService::ci05SetVideoSource(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetVideoSource, {{"value", value}}, context, cb);
}

void IrService::ci05SetParamLine(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetParamLine, {{"value", value}}, context, cb);
}

void IrService::ci05SetDigitalFormat(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetDigitalFormat, {{"value", value}}, context, cb);
}

void IrService::ci05SetTestPattern(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetTestPattern, {{"value", value}}, context, cb);
}

void IrService::ci05SetImageMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetImageMode, {{"value", value}}, context, cb);
}

void IrService::ci05SetStatusOutputMode(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetStatusOutputMode, {{"value", value}}, context, cb);
}

void IrService::ci05SetTmodFilter(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetTmodFilter, {{"value", value}}, context, cb);
}

void IrService::ci05SetNtmFilter(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetNtmFilter, {{"value", value}}, context, cb);
}

void IrService::ci05SetVerticalStripeRemoval(QObject* context, quint8 value, Callback cb) const
{
    simpleCmd(RpcCommand::Ir::Ci05::SetVerticalStripeRemoval, {{"value", value}}, context, cb);
}

void IrService::ci05ReadSerialNumber(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadSerialNumber, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadWorkMinutes(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadWorkMinutes, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadCoolingDoneSeconds(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadCoolingDoneSeconds, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadStatus1(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadStatus1, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadStatus2(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadStatus2, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadStatus3(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadStatus3, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadStatus4(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadStatus4, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadWorkState(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadWorkState, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

void IrService::ci05ReadSelfCheck(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::Ir::Ci05::ReadSelfCheck, {}, context, [cb](const RpcResult& r) { callJson(cb, r); }, RpcTimeout::Slow);
}

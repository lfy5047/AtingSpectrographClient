#pragma once

#include "RpcServiceBase.h"

class IrService : public RpcServiceBase {
public:
    explicit IrService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void currentModel(QObject* context, JsonCallback cb) const;

    void triggerCalibration(QObject* context, Callback cb) const;
    void forceShutter(QObject* context, Callback cb) const;
    void getVersion(QObject* context, JsonCallback cb) const;
    void setImageType(QObject* context, quint8 value, Callback cb) const;
    void setTestPattern(QObject* context, quint8 value, Callback cb) const;
    void setColorMode(QObject* context, quint8 value, Callback cb) const;
    void setBadPixelDisplayMode(QObject* context, quint8 value, Callback cb) const;
    void setBrightness(QObject* context, quint8 v, Callback cb) const;
    void setContrast(QObject* context, quint8 v, Callback cb) const;
    void setAbMode(QObject* context, quint8 value, Callback cb) const;
    void setDde(QObject* context, quint8 value, Callback cb) const;
    void setTemporalFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const;
    void setMedianFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const;
    void setFlipHorizontal(QObject* context, quint8 value, Callback cb) const;
    void setFlipVertical(QObject* context, quint8 value, Callback cb) const;
    void setExternalSync(QObject* context, quint8 value, Callback cb) const;
    void setIntegration(QObject* context, quint16 v, Callback cb) const;
    void setManualIntegration(QObject* context, quint8 value, Callback cb) const;
    void setIntegrationGearMode(QObject* context, quint8 value, Callback cb) const;
    void selectIntegrationGear(QObject* context, quint8 value, Callback cb) const;
    void queryIntegrationTime(QObject* context, JsonCallback cb) const;
    void setStandby(QObject* context, quint8 value, Callback cb) const;
    void setOnboardAutoCalibration(QObject* context, quint8 value, Callback cb) const;
    void readModuleId(QObject* context, JsonCallback cb) const;
    void readSelfCheck(QObject* context, JsonCallback cb) const;
    void readFocusPlaneTemp(QObject* context, JsonCallback cb) const;
    void readMean(QObject* context, JsonCallback cb) const;
    void readCorrectionParamGear(QObject* context, JsonCallback cb) const;
    void readCoreTemp(QObject* context, JsonCallback cb) const;
    void readBadPixelCount(QObject* context, JsonCallback cb) const;
    void maintenanceUnlock(QObject* context, quint8 value, Callback cb) const;
    void maintenanceExec(QObject* context, const QString& name, quint8 value, JsonCallback cb) const;
    void twoPointCalibP1(QObject* context, Callback cb) const;
    void twoPointCalibP2(QObject* context, Callback cb) const;
    void saveCalibParams(QObject* context, JsonCallback cb) const;
    void clearK(QObject* context, quint8 value, Callback cb) const;
    void clearB(QObject* context, quint8 value, Callback cb) const;
    void badPixelSearch(QObject* context, quint8 value, Callback cb) const;
    void setBadPixelPosition(QObject* context, const quint8 pos[4], Callback cb) const;
    void saveBadPixel(QObject* context, Callback cb) const;

    void ci05FocusStartPositive(QObject* context, Callback cb) const;
    void ci05FocusStartNegative(QObject* context, Callback cb) const;
    void ci05FocusStop(QObject* context, Callback cb) const;
    void ci05FocusStepPositive(QObject* context, Callback cb) const;
    void ci05FocusStepNegative(QObject* context, Callback cb) const;
    void ci05ZoomStartPositive(QObject* context, Callback cb) const;
    void ci05ZoomStartNegative(QObject* context, Callback cb) const;
    void ci05ZoomStop(QObject* context, Callback cb) const;
    void ci05ZoomStepPositive(QObject* context, Callback cb) const;
    void ci05ZoomStepNegative(QObject* context, Callback cb) const;
    void ci05AutoFocus(QObject* context, Callback cb) const;
    void ci05SetFov(QObject* context, quint8 value, Callback cb) const;
    void ci05ShutterOpen(QObject* context, Callback cb) const;
    void ci05ShutterClose(QObject* context, Callback cb) const;
    void ci05SetFocusSpeed(QObject* context, quint8 value, Callback cb) const;
    void ci05SetZoomSpeed(QObject* context, quint8 value, Callback cb) const;
    void ci05CallPreset(QObject* context, quint8 value, Callback cb) const;
    void ci05SetPreset(QObject* context, quint8 value, Callback cb) const;
    void ci05SetFocalLengthMmX10(QObject* context, quint16 value, Callback cb) const;
    void ci05QueryFocalLength(QObject* context, JsonCallback cb) const;
    void ci05QueryFocusMotorPosition(QObject* context, JsonCallback cb) const;
    void ci05QueryZoomMotorPosition(QObject* context, JsonCallback cb) const;

    void ci05MenuUser(QObject* context, Callback cb) const;
    void ci05MenuRight(QObject* context, Callback cb) const;
    void ci05MenuLeft(QObject* context, Callback cb) const;
    void ci05MenuParamInc(QObject* context, Callback cb) const;
    void ci05MenuParamDec(QObject* context, Callback cb) const;
    void ci05PromptOn(QObject* context, Callback cb) const;
    void ci05PromptOff(QObject* context, Callback cb) const;
    void ci05SetSyncMode(QObject* context, quint8 value, Callback cb) const;

    void ci05SetBrightness(QObject* context, quint8 value, Callback cb) const;
    void ci05SetContrast(QObject* context, quint8 value, Callback cb) const;
    void ci05SetOverallBrightness(QObject* context, quint8 value, Callback cb) const;
    void ci05SetOverallContrast(QObject* context, quint8 value, Callback cb) const;
    void ci05SetSharpness(QObject* context, quint8 value, Callback cb) const;
    void ci05SetY8Level(QObject* context, quint8 value, Callback cb) const;
    void ci05SetEzoom(QObject* context, quint8 value, Callback cb) const;
    void ci05SetFreeze(QObject* context, quint8 value, Callback cb) const;
    void ci05SetMirrorMode(QObject* context, quint8 value, Callback cb) const;
    void ci05SetPolarityPalette(QObject* context, quint8 value, Callback cb) const;
    void ci05SetAgcMode(QObject* context, quint8 value, Callback cb) const;
    void ci05SaveParams(QObject* context, Callback cb) const;

    void ci05SetIntegrationMsX10(QObject* context, quint16 value, Callback cb) const;
    void ci05IntegrationIncrease0p1Ms(QObject* context, Callback cb) const;
    void ci05IntegrationDecrease0p1Ms(QObject* context, Callback cb) const;
    void ci05SetIntegrationMc(QObject* context, quint32 value, Callback cb) const;
    void ci05SetFrameRateHzX100(QObject* context, quint16 value, Callback cb) const;
    void ci05ReadFrameRateHz(QObject* context, JsonCallback cb) const;
    void ci05SetIntegrationGear(QObject* context, quint8 value, Callback cb) const;
    void ci05SetIntegrationGearAuto(QObject* context, quint8 value, Callback cb) const;
    void ci05SetBackgroundGear(QObject* context, quint8 value, Callback cb) const;
    void ci05SetBackgroundGearAuto(QObject* context, quint8 value, Callback cb) const;
    void ci05TriggerShutterCompensation(QObject* context, Callback cb) const;
    void ci05TriggerSceneCompensation(QObject* context, Callback cb) const;
    void ci05TriggerDefocusCompensation(QObject* context, Callback cb) const;
    void ci05TriggerIntegrationCorrection(QObject* context, Callback cb) const;
    void ci05SetBootCompensationMode(QObject* context, quint8 value, Callback cb) const;
    void ci05SetGearSwitchCompensationMode(QObject* context, quint8 value, Callback cb) const;
    void ci05SetVideoSource(QObject* context, quint8 value, Callback cb) const;
    void ci05SetParamLine(QObject* context, quint8 value, Callback cb) const;
    void ci05SetDigitalFormat(QObject* context, quint8 value, Callback cb) const;
    void ci05SetTestPattern(QObject* context, quint8 value, Callback cb) const;
    void ci05SetImageMode(QObject* context, quint8 value, Callback cb) const;
    void ci05SetStatusOutputMode(QObject* context, quint8 value, Callback cb) const;
    void ci05SetTmodFilter(QObject* context, quint8 value, Callback cb) const;
    void ci05SetNtmFilter(QObject* context, quint8 value, Callback cb) const;
    void ci05SetVerticalStripeRemoval(QObject* context, quint8 value, Callback cb) const;

    void ci05ReadSerialNumber(QObject* context, JsonCallback cb) const;
    void ci05ReadWorkMinutes(QObject* context, JsonCallback cb) const;
    void ci05ReadCoolingDoneSeconds(QObject* context, JsonCallback cb) const;
    void ci05ReadStatus1(QObject* context, JsonCallback cb) const;
    void ci05ReadStatus2(QObject* context, JsonCallback cb) const;
    void ci05ReadStatus3(QObject* context, JsonCallback cb) const;
    void ci05ReadStatus4(QObject* context, JsonCallback cb) const;
    void ci05ReadWorkState(QObject* context, JsonCallback cb) const;
    void ci05ReadSelfCheck(QObject* context, JsonCallback cb) const;
};

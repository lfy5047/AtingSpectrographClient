#pragma once

#include "RpcServiceBase.h"

class IrService : public RpcServiceBase {
public:
    explicit IrService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    // 基础标定
    void triggerCalibration(QObject* context, Callback cb) const;
    void forceShutter(QObject* context, Callback cb) const;
    // 版本
    void getVersion(QObject* context, JsonCallback cb) const;
    // 图像选择与显示
    void setImageType(QObject* context, quint8 value, Callback cb) const;
    void setTestPattern(QObject* context, quint8 value, Callback cb) const;
    void setColorMode(QObject* context, quint8 value, Callback cb) const;
    void setBadPixelDisplayMode(QObject* context, quint8 value, Callback cb) const;
    // 图像参数
    void setBrightness(QObject* context, quint8 v, Callback cb) const;
    void setContrast(QObject* context, quint8 v, Callback cb) const;
    void setAbMode(QObject* context, quint8 value, Callback cb) const;
    void setDde(QObject* context, quint8 value, Callback cb) const;
    void setTemporalFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const;
    void setMedianFilter(QObject* context, bool enable, quint8 coeff, Callback cb) const;
    // 翻转与同步
    void setFlipHorizontal(QObject* context, quint8 value, Callback cb) const;
    void setFlipVertical(QObject* context, quint8 value, Callback cb) const;
    void setExternalSync(QObject* context, quint8 value, Callback cb) const;
    // 积分时间
    void setIntegration(QObject* context, quint16 v, Callback cb) const;
    void setManualIntegration(QObject* context, quint8 value, Callback cb) const;
    void setIntegrationGearMode(QObject* context, quint8 value, Callback cb) const;
    void selectIntegrationGear(QObject* context, quint8 value, Callback cb) const;
    void queryIntegrationTime(QObject* context, JsonCallback cb) const;
    // 模式控制
    void setStandby(QObject* context, quint8 value, Callback cb) const;
    void setOnboardAutoCalibration(QObject* context, quint8 value, Callback cb) const;
    // 读取查询
    void readModuleId(QObject* context, JsonCallback cb) const;
    void readSelfCheck(QObject* context, JsonCallback cb) const;
    void readFocusPlaneTemp(QObject* context, JsonCallback cb) const;
    void readMean(QObject* context, JsonCallback cb) const;
    void readCorrectionParamGear(QObject* context, JsonCallback cb) const;
    void readCoreTemp(QObject* context, JsonCallback cb) const;
    void readBadPixelCount(QObject* context, JsonCallback cb) const;
    // 维护与校正
    void maintenanceUnlock(QObject* context, quint8 value, Callback cb) const;
    void maintenanceExec(QObject* context, const QString& name, quint8 value, JsonCallback cb) const;
    void twoPointCalibP1(QObject* context, Callback cb) const;
    void twoPointCalibP2(QObject* context, Callback cb) const;
    void saveCalibParams(QObject* context, JsonCallback cb) const;
    void clearK(QObject* context, quint8 value, Callback cb) const;
    void clearB(QObject* context, quint8 value, Callback cb) const;
    // 坏元管理
    void badPixelSearch(QObject* context, quint8 value, Callback cb) const;
    void setBadPixelPosition(QObject* context, const quint8 pos[4], Callback cb) const;
    void saveBadPixel(QObject* context, Callback cb) const;
};

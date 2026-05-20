#pragma once

#include "RpcServiceBase.h"

class MirrorService : public RpcServiceBase {
public:
    explicit MirrorService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void queryAngle(QObject* context, MirrorAngleCallback cb) const;
    void setSpeed(QObject* context, int s, int f, Callback cb) const;
    void setTarget(QObject* context, double angle, Callback cb) const;
    void setTargetAbsolute(QObject* context, double angle, Callback cb) const;
    void startMove(QObject* context, Callback cb) const;
    void stopMove(QObject* context, Callback cb) const;
    void home(QObject* context, Callback cb) const;
    void setHome(QObject* context, Callback cb) const;
    void gotoPreset(QObject* context, int id, Callback cb) const;
};

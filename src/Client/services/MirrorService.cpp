#include "MirrorService.h"

void MirrorService::queryAngle(QObject* context, MirrorAngleCallback cb) const
{
    request(RpcCommand::Mirror::QueryAngle, {}, context, [cb](const RpcResult& r) {
        double angle = 0.0;
        bool moving = false;
        if (r.ok) {
            angle = r.data.value("angle", 0.0);
            moving = r.data.value("is_moving", false);
        }
        if (cb) cb(r.ok, angle, moving, r.msg);
    }, RpcTimeout::Slow);
}

void MirrorService::setSpeed(QObject* context, int s, int f, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::SetSpeed, {{"s_speed", s}, {"f_speed", f}}, context, cb, RpcTimeout::Slow);
}

void MirrorService::setTarget(QObject* context, double angle, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::SetTarget, {{"angle", angle}}, context, cb, RpcTimeout::Slow);
}

void MirrorService::setTargetAbsolute(QObject* context, double angle, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::SetTargetAbsolute, {{"angle", angle}}, context, cb, RpcTimeout::Slow);
}

void MirrorService::startMove(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::StartMove, {}, context, cb, RpcTimeout::Slow);
}

void MirrorService::stopMove(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::StopMove, {}, context, cb, RpcTimeout::Slow);
}

void MirrorService::home(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::Home, {}, context, cb, RpcTimeout::Slow);
}

void MirrorService::setHome(QObject* context, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::SetHome, {}, context, cb, RpcTimeout::Slow);
}

void MirrorService::gotoPreset(QObject* context, int id, Callback cb) const
{
    simpleCmd(RpcCommand::Mirror::GotoPreset, {{"id", id}}, context, cb, RpcTimeout::Slow);
}

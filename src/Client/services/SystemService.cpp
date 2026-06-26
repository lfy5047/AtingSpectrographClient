#include "SystemService.h"
#include <string>

void SystemService::ping(QObject* context, PingCallback cb) const
{
    request(RpcCommand::System::Ping, {}, context, [cb](const RpcResult& r) {
        qint64 ts = 0;
        if (r.ok) ts = r.data.value("ts", static_cast<qint64>(0));
        if (cb) cb(r.ok, ts, r.msg);
    });
}

void SystemService::version(QObject* context, VersionCallback cb) const
{
    request(RpcCommand::System::Version, {}, context, [cb](const RpcResult& r) {
        int ver = 0;
        QString name;
        if (r.ok) {
            ver = r.data.value("version", 0);
            name = QString::fromStdString(r.data.value("name", std::string()));
        }
        if (cb) cb(r.ok, ver, name, r.msg);
    });
}

void SystemService::status(QObject* context, JsonCallback cb) const
{
    request(RpcCommand::System::Status, {}, context, [cb](const RpcResult& r) {
        if (cb) cb(r.ok, r.data, r.msg);
    });
}

void SystemService::startBackgroundCalibration(QObject* context, BackgroundCalibrationStartCallback cb) const
{
    request(RpcCommand::System::BackgroundCalibrationStart, {}, context, [cb](const RpcResult& r) {
        BackgroundCalibrationStart result;
        if (r.ok) {
            result.started = r.data.value("started", false);
            result.taskId = r.data.value("task_id", 0);
            result.stage = QString::fromStdString(r.data.value("stage", std::string()));
        }
        if (cb) cb(r.ok, result, r.msg);
    });
}

void SystemService::backgroundCalibrationStatus(QObject* context, BackgroundCalibrationStatusCallback cb) const
{
    request(RpcCommand::System::BackgroundCalibrationStatus, {}, context, [cb](const RpcResult& r) {
        BackgroundCalibrationStatus status;
        if (r.ok) {
            status.taskId = r.data.value("task_id", 0);
            status.running = r.data.value("running", false);
            status.stage = QString::fromStdString(r.data.value("stage", std::string()));
            status.error = QString::fromStdString(r.data.value("error", std::string()));
            status.startedAtMs = r.data.value("started_at_ms", static_cast<qint64>(0));
            status.finishedAtMs = r.data.value("finished_at_ms", static_cast<qint64>(0));
        }
        if (cb) cb(r.ok, status, r.msg);
    });
}

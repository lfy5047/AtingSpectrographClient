#pragma once

#include <functional>

#include "RpcServiceBase.h"

struct BackgroundCalibrationStart {
    bool started = false;
    int taskId = 0;
    QString stage;
};

struct BackgroundCalibrationStatus {
    int taskId = 0;
    bool running = false;
    QString stage;
    QString error;
    qint64 startedAtMs = 0;
    qint64 finishedAtMs = 0;
};

class SystemService : public RpcServiceBase {
public:
    using BackgroundCalibrationStartCallback =
        std::function<void(bool, const BackgroundCalibrationStart&, const QString&)>;
    using BackgroundCalibrationStatusCallback =
        std::function<void(bool, const BackgroundCalibrationStatus&, const QString&)>;

    explicit SystemService(ControlClient* ctrl)
        : RpcServiceBase(ctrl)
    {
    }

    void ping(QObject* context, PingCallback cb) const;
    void version(QObject* context, VersionCallback cb) const;
    void status(QObject* context, JsonCallback cb) const;
    void startBackgroundCalibration(QObject* context, BackgroundCalibrationStartCallback cb) const;
    void backgroundCalibrationStatus(QObject* context, BackgroundCalibrationStatusCallback cb) const;
};

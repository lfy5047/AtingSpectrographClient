#pragma once

#include <functional>
#include <vector>
#include <QtGlobal>
#include <QString>
#include "json.hpp"

using Callback = std::function<void(bool ok, const QString& err)>;
using JsonCallback = std::function<void(bool ok, const nlohmann::json& data, const QString& err)>;
using PingCallback = std::function<void(bool ok, qint64 ts, const QString& err)>;
using VersionCallback = std::function<void(bool ok, int ver, const QString& name, const QString& err)>;
using MirrorAngleCallback = std::function<void(bool ok, double angle, bool moving, const QString& err)>;
using ResolutionCallback = std::function<void(bool ok, int w, int h, const QString& err)>;
using CollectStatusCallback = std::function<void(bool ok, bool collecting, const QString& err)>;

struct CollectOversamplingInfo {
    int oversampleFactor = 1;
    int effectiveSSpeed = 0;
    int effectiveFSpeed = 0;
    bool collecting = false;
};

using CollectOversamplingCallback = std::function<void(bool ok, const CollectOversamplingInfo& info, const QString& err)>;

struct CameraDeviceOption {
    QString name;
    QString mac;
};

using CameraSelectedDeviceCallback = std::function<void(bool ok, const QString& mac, const QString& err)>;
using CameraDeviceOptionsCallback = std::function<void(bool ok, const std::vector<CameraDeviceOption>& options, const QString& err)>;

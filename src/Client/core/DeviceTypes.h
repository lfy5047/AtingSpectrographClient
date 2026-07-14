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

struct CollectGateConfig {
    int discardFrontMs = 0;
    int discardBackMs = 0;
    int forwardOffsetFrames = 0;
    int reverseOffsetFrames = 0;
    bool staticCollectMode = false;
    bool collecting = false;
    bool pendingConfig = false;
};

using CollectGateConfigCallback = std::function<void(bool ok, const CollectGateConfig& config, const QString& err)>;

struct BinningConfig {
    bool enabled = false;
    int spectralFactor = 1;
    int spatialFactor = 1;
};

using BinningConfigCallback = std::function<void(bool ok, const BinningConfig& config, const QString& err)>;

struct CameraDeviceOption {
    QString name;
    QString mac;
};

using CameraSelectedDeviceCallback = std::function<void(bool ok, const QString& mac, const QString& err)>;
using CameraDeviceOptionsCallback = std::function<void(bool ok, const std::vector<CameraDeviceOption>& options, const QString& err)>;

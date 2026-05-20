#pragma once

#include <functional>
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

#pragma once

#include "ImageFrameUtils.h"

#include <QByteArray>
#include <QtGlobal>

struct RoiSnapshot {
    int width = 0;
    int height = 0;
    quint64 streamFrameId = 0;
    QByteArray data;
    ChannelImageStats stats;
};

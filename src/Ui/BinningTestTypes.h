#pragma once

#include "ImageFrameUtils.h"

#include <QByteArray>
#include <QSize>
#include <QtGlobal>

struct BinningSnapshot {
    int factor = 1;
    int width = 0;
    int height = 0;
    quint64 streamFrameId = 0;
    QByteArray data;
    ChannelImageStats stats;
};

bool isSupportedBinningFactor(int factor);
QSize expectedBinningSize(const QSize& baseline, int factor);
bool binningFeatureWidthMatches(int baselinePixels, int actualPixels,
                                int factor, int tolerancePixels = 1);

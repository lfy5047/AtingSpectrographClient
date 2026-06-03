#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QtGlobal>

struct StreamFrame {
    int channel = 0;
    int width = 0;
    int height = 0;
    int pixfmt = 0;
    quint64 streamFrameId = 0;
    quint8 frameType = 0;
    bool hasScanDirection = false;
    bool reverseScan = false;
    QByteArray data;
};

Q_DECLARE_METATYPE(StreamFrame)

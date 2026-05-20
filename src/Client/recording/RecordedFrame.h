#pragma once

#include <QByteArray>
#include <QMetaType>

struct RecordedFrame {
    int channel = 0;
    int width = 0;
    int height = 0;
    int pixfmt = 0;
    quint64 frameIndex = 0;
    quint64 timestampMs = 0;
    QByteArray data;
    quint32 crc32 = 0;
};

Q_DECLARE_METATYPE(RecordedFrame)

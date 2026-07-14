#pragma once

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <QtGlobal>

struct Mono16Stats {
    quint16 min = 0;
    quint16 max = 0;
    quint64 sum = 0;
};

struct ChannelImageStats {
    bool valid = false;
    quint16 min = 0;
    quint16 max = 0;
    double avg = 0.0;
};
Q_DECLARE_METATYPE(ChannelImageStats)

struct ComputeImageResult {
    QImage display;
    ChannelImageStats stats;
};

bool computeMono16Stats(const QByteArray& data, int width, int height, Mono16Stats* out);
QImage makeDisplayImage(int width, int height, int pixfmt, const QByteArray& data);
QImage makeMono16DisplayImage(int width, int height, const QByteArray& data,
                              quint16 displayMin, quint16 displayMax);
ChannelImageStats makeChannelImageStats(int width, int height, int pixfmt, const QByteArray& data);
ComputeImageResult computeImageResult(int width, int height, int pixfmt, const QByteArray& data);

#pragma once

#include <QObject>
#include <QByteArray>
#include <QBitArray>
#include <QMap>
#include <QTimer>
#include <cstdint>
#include "Protocol.h"

class FrameAssembler : public QObject {
    Q_OBJECT
public:
    explicit FrameAssembler(QObject* parent = nullptr);

    void feedPacket(const cli::proto::StreamHeader& hdr, const char* payload, int payloadLen);

    quint64 framesReceived() const { return framesReceived_; }
    quint64 framesDropped()  const { return framesDropped_; }

signals:
    void frameReady(int channel, int width, int height, int pixfmt, QByteArray data);

private:
    struct Slot {
        QByteArray buf;
        QBitArray  got;
        uint16_t   pktTotal    = 0;
        uint32_t   frameBytes  = 0;
        uint16_t   width       = 0;
        uint16_t   height      = 0;
        uint16_t   pixfmt      = 0;
        uint8_t    channel     = 0;
        int        payloadSize = 0;
        int        received    = 0;
        qint64     created     = 0;
    };

    void purgeStale();

    QMap<uint64_t, Slot> slots_;
    QTimer* purgeTimer_ = nullptr;
    quint64 framesReceived_ = 0;
    quint64 framesDropped_  = 0;

    static const int kMaxSlots   = 32;
    static const int kTimeoutMs  = 300;
};

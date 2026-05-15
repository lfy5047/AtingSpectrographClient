#include "FrameAssembler.h"
#include <QDateTime>
#include <cstring>

FrameAssembler::FrameAssembler(QObject* parent)
    : QObject(parent)
{
    purgeTimer_ = new QTimer(this);
    purgeTimer_->setInterval(100);
    connect(purgeTimer_, &QTimer::timeout, this, &FrameAssembler::purgeStale);
    purgeTimer_->start();
}

void FrameAssembler::feedPacket(const cli::proto::StreamHeader& hdr,
                                 const char* payload, int payloadLen)
{
    uint64_t fid = hdr.frame_id;

    auto it = slots_.find(fid);
    if (it == slots_.end()) {
        if (slots_.size() >= kMaxSlots) {
            auto oldest = slots_.begin();
            slots_.erase(oldest);
            ++framesDropped_;
        }

        Slot s;
        s.pktTotal   = hdr.pkt_total;
        s.frameBytes = hdr.frame_bytes;
        s.width      = hdr.width;
        s.height     = hdr.height;
        s.pixfmt     = hdr.pixel_format;
        s.channel    = hdr.channel;
        s.buf.resize(static_cast<int>(hdr.frame_bytes));
        s.buf.fill('\0');
        s.got.resize(hdr.pkt_total);
        s.got.fill(false);
        s.received   = 0;
        s.created    = QDateTime::currentMSecsSinceEpoch();
        s.payloadSize = 0;

        it = slots_.insert(fid, s);
    }

    Slot& slot = it.value();
    int idx = hdr.pkt_index;
    if (idx >= slot.pktTotal) return;
    if (slot.got.testBit(idx)) return;

    if (slot.payloadSize == 0 && !(hdr.flags & cli::proto::LastFragment)) {
        slot.payloadSize = payloadLen;
    }

    int offset = 0;
    if (slot.payloadSize > 0) {
        offset = idx * slot.payloadSize;
    } else {
        offset = idx * payloadLen;
    }

    int copyLen = qMin(payloadLen, static_cast<int>(slot.frameBytes) - offset);
    if (copyLen > 0 && offset + copyLen <= slot.buf.size()) {
        std::memcpy(slot.buf.data() + offset, payload, copyLen);
    }

    slot.got.setBit(idx);
    ++slot.received;

    if (slot.received >= slot.pktTotal) {
        ++framesReceived_;
        emit frameReady(slot.channel, slot.width, slot.height, slot.pixfmt, slot.buf);
        slots_.erase(it);
    }
}

void FrameAssembler::purgeStale()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = slots_.begin();
    while (it != slots_.end()) {
        if (now - it.value().created > kTimeoutMs) {
            ++framesDropped_;
            it = slots_.erase(it);
        } else {
            ++it;
        }
    }
}

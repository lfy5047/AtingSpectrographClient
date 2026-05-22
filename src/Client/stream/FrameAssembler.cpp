#include "FrameAssembler.h"
#include "plog/Log.h"
#include <QDateTime>
#include <algorithm>
#include <cstring>
#include <limits>

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
    if (hdr.pkt_total == 0 || hdr.pkt_index >= hdr.pkt_total || hdr.frame_bytes == 0 ||
        static_cast<qint64>(hdr.frame_bytes) > kMaxBufferedBytes) {
        PLOGW << "FrameAssembler: invalid header channel=" << static_cast<int>(hdr.channel)
              << " fid=" << hdr.frame_id
              << " idx=" << hdr.pkt_index
              << " pktTotal=" << hdr.pkt_total
              << " frameBytes=" << hdr.frame_bytes;
        return;
    }

    const FrameKey key(hdr.channel, hdr.frame_id);
    const bool isLast = (hdr.flags & cli::proto::LastFragment) != 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    auto it = slots_.find(key);
    if (it == slots_.end()) {
        while (slots_.size() >= kMaxSlots ||
               bufferedBytes_ + static_cast<qint64>(hdr.frame_bytes) > kMaxBufferedBytes) {
            dropOldestSlot("slot overflow");
        }

        Slot s;
        s.pktTotal = hdr.pkt_total;
        s.frameBytes = hdr.frame_bytes;
        s.width = hdr.width;
        s.height = hdr.height;
        s.pixfmt = hdr.pixel_format;
        s.channel = hdr.channel;
        s.streamFrameId = hdr.frame_id;
        s.frameType = hdr.frame_type;
        s.buf.resize(static_cast<int>(hdr.frame_bytes));
        s.buf.fill('\0');
        s.got.resize(hdr.pkt_total);
        s.got.fill(false);
        s.received = 0;
        s.payloadSize = 0;
        s.created = now;
        s.lastSeen = now;

        it = slots_.insert(key, s);
        bufferedBytes_ += s.buf.size();
    }

    Slot& slot = it.value();
    slot.lastSeen = now;

    if (slot.pktTotal != hdr.pkt_total || slot.frameBytes != hdr.frame_bytes ||
        slot.width != hdr.width || slot.height != hdr.height ||
        slot.pixfmt != hdr.pixel_format || slot.frameType != hdr.frame_type) {
        PLOGW << "FrameAssembler: metadata changed channel=" << static_cast<int>(hdr.channel)
              << " fid=" << hdr.frame_id
              << " oldTotal=" << slot.pktTotal << " newTotal=" << hdr.pkt_total
              << " oldBytes=" << slot.frameBytes << " newBytes=" << hdr.frame_bytes
              << " oldFrameType=" << static_cast<int>(slot.frameType)
              << " newFrameType=" << static_cast<int>(hdr.frame_type);
        bufferedBytes_ -= slot.buf.size();
        slots_.erase(it);
        ++framesDropped_;
        return;
    }

    const int idx = hdr.pkt_index;
    if (slot.got.testBit(idx)) return;

    int offset = 0;
    if (isLast) {
        offset = static_cast<int>(slot.frameBytes) - payloadLen;
        if (offset < 0) offset = 0;
    } else {
        if (slot.payloadSize == 0) slot.payloadSize = payloadLen;
        offset = idx * slot.payloadSize;
    }

    const int remaining = static_cast<int>(slot.frameBytes) - offset;
    const int copyLen = std::min(payloadLen, remaining);
    if (copyLen > 0 && offset >= 0 && offset + copyLen <= slot.buf.size()) {
        std::memcpy(slot.buf.data() + offset, payload, copyLen);
    } else {
        PLOGW << "FrameAssembler: memcpy skipped channel=" << static_cast<int>(hdr.channel)
              << " fid=" << hdr.frame_id
              << " idx=" << idx
              << " offset=" << offset
              << " copyLen=" << copyLen
              << " bufSize=" << slot.buf.size()
              << " frameBytes=" << slot.frameBytes
              << " payloadLen=" << payloadLen
              << " last=" << isLast;
    }

    slot.got.setBit(idx);
    ++slot.received;

    if (slot.received >= slot.pktTotal) {
        ++framesReceived_;
        StreamFrame frame;
        frame.channel = slot.channel;
        frame.width = slot.width;
        frame.height = slot.height;
        frame.pixfmt = slot.pixfmt;
        frame.streamFrameId = slot.streamFrameId;
        frame.frameType = slot.frameType;
        frame.data = slot.buf;
        emit frameReady(frame);
        bufferedBytes_ -= slot.buf.size();
        slots_.erase(it);
    }
}

void FrameAssembler::purgeStale()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = slots_.begin();
    while (it != slots_.end()) {
        if (now - it.value().lastSeen > kTimeoutMs) {
            PLOGW << "FrameAssembler: purge stale channel=" << static_cast<int>(it.key().channel)
                  << " fid=" << it.key().frameId
                  << " received=" << it.value().received
                  << "/" << it.value().pktTotal
                  << " age=" << (now - it.value().created) << "ms"
                  << " idle=" << (now - it.value().lastSeen) << "ms";
            ++framesDropped_;
            bufferedBytes_ -= it.value().buf.size();
            it = slots_.erase(it);
        } else {
            ++it;
        }
    }
}

void FrameAssembler::dropOldestSlot(const char* reason)
{
    if (slots_.isEmpty()) return;

    auto oldest = slots_.begin();
    qint64 oldestCreated = std::numeric_limits<qint64>::max();
    for (auto it = slots_.begin(); it != slots_.end(); ++it) {
        if (it.value().created < oldestCreated) {
            oldest = it;
            oldestCreated = it.value().created;
        }
    }

    PLOGW << "FrameAssembler: " << reason
          << ", dropping oldest channel=" << static_cast<int>(oldest.key().channel)
          << " fid=" << oldest.key().frameId
          << " received=" << oldest.value().received << "/" << oldest.value().pktTotal
          << " totalSlots=" << slots_.size()
          << " bufferedBytes=" << bufferedBytes_;
    bufferedBytes_ -= oldest.value().buf.size();
    slots_.erase(oldest);
    ++framesDropped_;
}

#include "FrameAssembler.h"
#include "plog/Log.h"
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
    bool isLast = (hdr.flags & cli::proto::LastFragment) != 0;


    auto it = slots_.find(fid);
    if (it == slots_.end()) {
        if (slots_.size() >= kMaxSlots) {
            auto oldest = slots_.begin();
            PLOGW << "FrameAssembler: slot overflow, dropping oldest fid="
                  << oldest.key() << " totalSlots=" << slots_.size();
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

    if (idx >= slot.pktTotal) {
        PLOGW << "FrameAssembler: idx out of range fid=" << fid
              << " idx=" << idx << " pktTotal=" << slot.pktTotal;
        return;
    }
    if (slot.got.testBit(idx)) return;

    // --- offset calculation ---
    int offset = 0;
    if (isLast) {
        // 最后一片段：根据帧尾部计算偏移量。
        offset = static_cast<int>(slot.frameBytes) - payloadLen;
        if (offset < 0) offset = 0;
    } else {
        // 非最后一片段：从第一个片段 payloadSize。
        if (slot.payloadSize == 0)
            slot.payloadSize = payloadLen;
        offset = idx * slot.payloadSize;
    }

    int copyLen = qMin(payloadLen, static_cast<int>(slot.frameBytes) - offset);
    if (copyLen > 0 && offset + copyLen <= slot.buf.size()) {
        std::memcpy(slot.buf.data() + offset, payload, copyLen);
    } else {
        PLOGW << "FrameAssembler: memcpy skipped fid=" << fid << " idx=" << idx
              << " offset=" << offset << " copyLen=" << copyLen
              << " bufSize=" << slot.buf.size() << " frameBytes=" << slot.frameBytes
              << " payloadLen=" << payloadLen << " last=" << isLast;
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
            PLOGW << "FrameAssembler: purge stale fid=" << it.key()
                  << " received=" << it.value().received
                  << "/" << it.value().pktTotal
                  << " age=" << (now - it.value().created) << "ms";
            ++framesDropped_;
            it = slots_.erase(it);
        } else {
            ++it;
        }
    }
}

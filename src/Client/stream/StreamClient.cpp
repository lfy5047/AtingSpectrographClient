#include "StreamClient.h"
#include <cmath>
#include <cstring>
#include <QDateTime>
#include <QHostAddress>
#include "plog/Log.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

using namespace cli::proto;
namespace {
const qint64 kDropLogIntervalMs = 1000;
const uint8_t kMinStreamChannel = 1;
const uint8_t kMaxStreamChannel = 4;
const uint8_t kMinSupportedStreamVersion = 2;
}

StreamClient::StreamClient(QObject* parent)
    : QObject(parent)
{
    connect(&sock_, &QUdpSocket::readyRead, this, &StreamClient::onReadyRead);
    connect(&assembler_, &FrameAssembler::frameReady, this, [this](const StreamFrame& frame) {
        framesByChannel_[frame.channel] = framesByChannel_.value(frame.channel, 0) + 1;
        emit frameReady(frame);
    });

    fpsTimer_.setInterval(1000);
    connect(&fpsTimer_, &QTimer::timeout, this, &StreamClient::updateFps);
}

bool StreamClient::bind(quint16 localPort)
{
    close();
    bound_ = sock_.bind(QHostAddress::Any, localPort, QUdpSocket::ShareAddress);
    if (bound_) {
        constexpr int kRcvBuf = 1024 * 1024 * 64;
#ifdef Q_OS_WIN
        int val = kRcvBuf;
        ::setsockopt(static_cast<SOCKET>(sock_.socketDescriptor()),
                     SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<const char*>(&val), sizeof(val));
#else
        sock_.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, kRcvBuf);
#endif
        int actual = sock_.socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption).toInt();
        PLOGI << "StreamClient: SO_RCVBUF requested=" << kRcvBuf << " actual=" << actual;
        port_ = localPort;
        fpsTimer_.start();
    }
    return bound_;
}

void StreamClient::close()
{
    if (bound_) {
        fpsTimer_.stop();
        sock_.close();
        bound_ = false;
        port_ = 0;
        fps_ = 0;
        lastFrameCount_ = 0;
        framesByChannel_.clear();
        lastFrameCountByChannel_.clear();
        fpsByChannel_.clear();
        emit statsUpdated();
    }
}

void StreamClient::onReadyRead()
{
    QByteArray dg;
    while (sock_.hasPendingDatagrams()) {
        dg.resize(static_cast<int>(sock_.pendingDatagramSize()));
        sock_.readDatagram(dg.data(), dg.size());

        if (dg.size() < static_cast<int>(sizeof(StreamHeader))) {
            logDropRateLimited("short datagram", nullptr, dg.size());
            continue;
        }

        StreamHeader hdr;
        std::memcpy(&hdr, dg.constData(), sizeof(hdr));

        if (hdr.magic != kStreamMagic) {
            logDropRateLimited("bad stream magic", &hdr, dg.size());
            continue;
        }
        if (hdr.version < kMinSupportedStreamVersion || hdr.version > kStreamProtoVersion) {
            logDropRateLimited("unsupported stream version", &hdr, dg.size());
            continue;
        }
        if (hdr.channel < kMinStreamChannel || hdr.channel > kMaxStreamChannel) {
            logDropRateLimited("invalid stream channel", &hdr, dg.size());
            continue;
        }

        const char* payload = dg.constData() + sizeof(StreamHeader);
        int payloadLen = dg.size() - static_cast<int>(sizeof(StreamHeader));
        if (payloadLen < 0) {
            logDropRateLimited("negative payload length", &hdr, dg.size());
            continue;
        }

        if (hdr.frame_type > TailFrame) {
            logDropRateLimited("invalid frame_type", &hdr, dg.size());
            continue;
        }

        const bool hasMeta = (hdr.meta_flags & HasRawFrameMeta) != 0;
        const bool hasDirection = hdr.version >= kStreamProtoVersion &&
                                  (hdr.meta_flags & HasScanDirection) != 0;
        if (hasMeta) {
            if (hdr.is_latest_mirror_frame > 1) {
                logDropRateLimited("invalid is_latest_mirror_frame", &hdr, dg.size());
                continue;
            }
            double mirrorAngle = 0.0;
            std::memcpy(&mirrorAngle, &hdr.mirror_angle_bits, sizeof(mirrorAngle));
            (void)mirrorAngle;
            (void)hdr.timestamp_ns;
            (void)hdr.mirror_timestamp_ns;

        }
        assembler_.feedPacket(hdr, payload, payloadLen);
    }
}

void StreamClient::updateFps()
{
    quint64 cur = assembler_.framesReceived();
    fps_ = static_cast<double>(cur - lastFrameCount_);
    lastFrameCount_ = cur;

    for (auto it = framesByChannel_.cbegin(); it != framesByChannel_.cend(); ++it) {
        const int channel = it.key();
        const quint64 channelCur = it.value();
        const quint64 channelLast = lastFrameCountByChannel_.value(channel, 0);
        fpsByChannel_[channel] = static_cast<double>(channelCur - channelLast);
        lastFrameCountByChannel_[channel] = channelCur;
    }
    emit statsUpdated();
}

void StreamClient::logDropRateLimited(const char* reason, const cli::proto::StreamHeader* hdr, int datagramBytes)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (dropLogLastMs_ == 0 || nowMs - dropLogLastMs_ >= kDropLogIntervalMs) {
        if (hdr) {
            PLOGW << "StreamClient: drop packet reason=" << reason
                  << " ch=" << static_cast<int>(hdr->channel)
                  << " ver=" << static_cast<int>(hdr->version)
                  << " fid=" << hdr->frame_id
                  << " idx=" << hdr->pkt_index
                  << " total=" << hdr->pkt_total
                  << " meta=" << hdr->meta_flags
                  << " frameType=" << static_cast<int>(hdr->frame_type)
                  << " reverseScan=" << static_cast<int>(hdr->reverse_scan)
                  << " dgBytes=" << datagramBytes
                  << " suppressed=" << dropLogSuppressed_;
        } else {
            PLOGW << "StreamClient: drop packet reason=" << reason
                  << " dgBytes=" << datagramBytes
                  << " suppressed=" << dropLogSuppressed_;
        }
        dropLogLastMs_ = nowMs;
        dropLogSuppressed_ = 0;
        return;
    }

    ++dropLogSuppressed_;
}

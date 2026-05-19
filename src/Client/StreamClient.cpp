#include "StreamClient.h"
#include <cstring>
#include <QHostAddress>
#include "plog/Log.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

using namespace cli::proto;

StreamClient::StreamClient(QObject* parent)
    : QObject(parent)
{
    connect(&sock_, &QUdpSocket::readyRead, this, &StreamClient::onReadyRead);
    connect(&assembler_, &FrameAssembler::frameReady, this, &StreamClient::frameReady);

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
    }
}

void StreamClient::onReadyRead()
{
    QByteArray dg;
    while (sock_.hasPendingDatagrams()) {
        dg.resize(static_cast<int>(sock_.pendingDatagramSize()));
        sock_.readDatagram(dg.data(), dg.size());

        if (dg.size() < static_cast<int>(sizeof(StreamHeader))) continue;

        StreamHeader hdr;
        std::memcpy(&hdr, dg.constData(), sizeof(hdr));

        if (hdr.magic != kStreamMagic || hdr.version != kProtoVersion) continue;

        const char* payload = dg.constData() + sizeof(StreamHeader);
        int payloadLen = dg.size() - static_cast<int>(sizeof(StreamHeader));

        assembler_.feedPacket(hdr, payload, payloadLen);
    }
}

void StreamClient::updateFps()
{
    quint64 cur = assembler_.framesReceived();
    fps_ = static_cast<double>(cur - lastFrameCount_);
    lastFrameCount_ = cur;
}

#include "StreamClient.h"
#include <cstring>
#include <QHostAddress>

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
    sock_.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 8 * 1024 * 1024);
    bound_ = sock_.bind(QHostAddress::Any, localPort, QUdpSocket::ShareAddress);
    if (bound_) {
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
    while (sock_.hasPendingDatagrams()) {
        QByteArray dg;
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

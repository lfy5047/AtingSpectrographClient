#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstring>

#include "CollectService.h"
#include "ControlClient.h"
#include "Protocol.h"

class CollectServiceTest : public QObject {
    Q_OBJECT

private slots:
    void setOversamplingSendsFactor();
    void getOversamplingParsesServerData();
};

namespace {

struct CapturedRequest {
    cli::proto::CtrlHeader header;
    nlohmann::json payload;
};

CapturedRequest readRequest(QTcpSocket* socket)
{
    using namespace cli::proto;

    const int headerSize = static_cast<int>(sizeof(CtrlHeader));
    QElapsedTimer timer;
    timer.start();
    while (socket->bytesAvailable() < headerSize && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        socket->waitForReadyRead(50);
    }
    if (socket->bytesAvailable() < headerSize) return CapturedRequest();

    CapturedRequest req;
    std::memcpy(&req.header, socket->read(sizeof(CtrlHeader)).constData(), sizeof(CtrlHeader));
    timer.restart();
    while (socket->bytesAvailable() < req.header.payload_len) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        socket->waitForReadyRead(50);
        if (timer.elapsed() >= 1000) return req;
    }
    const QByteArray payloadBytes = socket->read(req.header.payload_len);
    req.payload = nlohmann::json::parse(payloadBytes.constData());
    return req;
}

void writeResponse(QTcpSocket* socket, uint32_t seq, const nlohmann::json& data)
{
    using namespace cli::proto;

    const std::string body = nlohmann::json{{"ok", true}, {"data", data}}.dump();
    QByteArray frame(static_cast<int>(sizeof(CtrlHeader) + body.size()), '\0');

    CtrlHeader header;
    header.magic = kCtrlMagic;
    header.version = kCtrlProtoVersion;
    header.type = static_cast<uint16_t>(Response);
    header.seq = seq;
    header.payload_len = static_cast<uint32_t>(body.size());

    std::memcpy(frame.data(), &header, sizeof(header));
    std::memcpy(frame.data() + sizeof(header), body.data(), body.size());
    socket->write(frame);
    QVERIFY(socket->waitForBytesWritten(1000));
}

QTcpSocket* connectClient(QTcpServer& server, ControlClient& client)
{
    if (!server.listen(QHostAddress::LocalHost, 0)) return nullptr;
    client.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    if (!server.waitForNewConnection(1000)) return nullptr;
    QTcpSocket* socket = server.nextPendingConnection();
    return socket;
}

}

void CollectServiceTest::setOversamplingSendsFactor()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    CollectService service(&client);
    service.setOversampling(nullptr, 2, nullptr);

    const CapturedRequest req = readRequest(socket);
    QCOMPARE(req.header.magic, cli::proto::kCtrlMagic);
    QCOMPARE(req.header.version, cli::proto::kCtrlProtoVersion);
    QCOMPARE(req.header.type, static_cast<uint16_t>(cli::proto::Request));
    QVERIFY(req.header.payload_len > 0);
    QCOMPARE(QString::fromStdString(req.payload.value("cmd", std::string())),
             QStringLiteral("collect.set_oversampling"));
    QVERIFY(req.payload.contains("params"));
    QCOMPARE(req.payload["params"].value("oversample_factor", 0), 2);
}

void CollectServiceTest::getOversamplingParsesServerData()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    CollectService service(&client);
    bool called = false;
    service.getOversampling(nullptr, [&called](bool ok, const CollectOversamplingInfo& info, const QString& err) {
        called = true;
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(info.oversampleFactor, 2);
        QCOMPARE(info.effectiveSSpeed, 200);
        QCOMPARE(info.effectiveFSpeed, 150);
        QVERIFY(info.collecting);
    });

    const CapturedRequest req = readRequest(socket);
    QCOMPARE(req.header.magic, cli::proto::kCtrlMagic);
    QCOMPARE(req.header.version, cli::proto::kCtrlProtoVersion);
    QCOMPARE(req.header.type, static_cast<uint16_t>(cli::proto::Request));
    QVERIFY(req.header.payload_len > 0);
    QCOMPARE(QString::fromStdString(req.payload.value("cmd", std::string())),
             QStringLiteral("collect.get_oversampling"));
    writeResponse(socket, req.header.seq,
                  {{"oversample_factor", 2},
                   {"effective_s_speed", 200},
                   {"effective_f_speed", 150},
                   {"is_collecting", true}});
    QTRY_VERIFY(called);
}

QTEST_MAIN(CollectServiceTest)

#include "CollectServiceTest.moc"

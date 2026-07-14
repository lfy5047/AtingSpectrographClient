#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstring>

#include "ControlClient.h"
#include "Protocol.h"
#include "RoiService.h"

class RoiServiceTest : public QObject {
    Q_OBJECT

private slots:
    void getConfigParsesServerData();
    void setConfigSendsAllFields();
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

    CapturedRequest request;
    const QByteArray headerBytes = socket->read(sizeof(CtrlHeader));
    if (headerBytes.size() != headerSize) return request;
    std::memcpy(&request.header, headerBytes.constData(), sizeof(CtrlHeader));

    timer.restart();
    while (socket->bytesAvailable() < request.header.payload_len && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        socket->waitForReadyRead(50);
    }
    if (socket->bytesAvailable() < request.header.payload_len) return request;

    request.payload = nlohmann::json::parse(socket->read(request.header.payload_len).constData());
    return request;
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
    return server.nextPendingConnection();
}

} // namespace

void RoiServiceTest::getConfigParsesServerData()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    RoiService service(&client);
    bool called = false;
    service.getConfig(nullptr, [&called](bool ok, const RoiConfig& config, const QString& err) {
        called = true;
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(config.sliceBegin, 240);
        QCOMPARE(config.sliceEnd, 435);
        QCOMPARE(config.sliceHBegin, 0);
        QCOMPARE(config.sliceHEnd, 512);
        QVERIFY(config.collecting);
        QVERIFY(config.pendingApply);
    });

    const CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("roi.get_config"));
    writeResponse(socket, request.header.seq,
                  {{"slice_begin", 240},
                   {"slice_end", 435},
                   {"slice_h_begin", 0},
                   {"slice_h_end", 512},
                   {"is_collecting", true},
                   {"pending_apply", true}});
    QTRY_VERIFY(called);
}

void RoiServiceTest::setConfigSendsAllFields()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    RoiService service(&client);
    RoiConfig config;
    config.sliceBegin = 240;
    config.sliceEnd = 435;
    config.sliceHBegin = 0;
    config.sliceHEnd = 512;
    service.setConfig(nullptr, config, nullptr);

    const CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("roi.set_config"));
    QVERIFY(request.payload.contains("params"));
    QCOMPARE(request.payload["params"].value("slice_begin", -1), 240);
    QCOMPARE(request.payload["params"].value("slice_end", -1), 435);
    QCOMPARE(request.payload["params"].value("slice_h_begin", -1), 0);
    QCOMPARE(request.payload["params"].value("slice_h_end", -1), 512);
}

QTEST_MAIN(RoiServiceTest)

#include "RoiServiceTest.moc"

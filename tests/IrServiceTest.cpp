#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstring>

#include "ControlClient.h"
#include "IrService.h"
#include "Protocol.h"

class IrServiceTest : public QObject {
    Q_OBJECT

private slots:
    void legacyWriteCommandsUseLegacyNamespace();
    void coreCurrentReturnsConfiguredModel();
    void ci05CommonCommandsUseCi05Namespace();
    void ci05StatusReadReturnsServerData();
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
    const QByteArray headerBytes = socket->read(sizeof(CtrlHeader));
    std::memcpy(&req.header, headerBytes.constData(), sizeof(CtrlHeader));
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
    return server.nextPendingConnection();
}

void verifyRequestFrame(const CapturedRequest& req, const QString& expectedCommand)
{
    QCOMPARE(req.header.magic, cli::proto::kCtrlMagic);
    QCOMPARE(req.header.version, cli::proto::kCtrlProtoVersion);
    QCOMPARE(req.header.type, static_cast<uint16_t>(cli::proto::Request));
    QVERIFY(req.header.payload_len > 0);
    QCOMPARE(QString::fromStdString(req.payload.value("cmd", std::string())),
             expectedCommand);
    QVERIFY(req.payload.contains("params"));
}

} // namespace

void IrServiceTest::legacyWriteCommandsUseLegacyNamespace()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    IrService service(&client);
    service.setBrightness(nullptr, 128, nullptr);

    CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.legacy.set_brightness"));
    QCOMPARE(req.payload["params"].value("value", -1), 128);

    service.maintenanceExec(nullptr, QStringLiteral("clear_b"), 1, nullptr);

    req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.legacy.maintenance_exec"));
    QCOMPARE(QString::fromStdString(req.payload["params"].value("name", std::string())),
             QStringLiteral("clear_b"));
    QCOMPARE(req.payload["params"].value("value", -1), 1);
}

void IrServiceTest::coreCurrentReturnsConfiguredModel()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    IrService service(&client);
    bool called = false;
    service.currentModel(nullptr, [&called](bool ok, const nlohmann::json& data, const QString& err) {
        called = true;
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(QString::fromStdString(data.value("model", std::string())),
                 QStringLiteral("ci05"));
    });

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.core.current"));
    QVERIFY(req.payload["params"].empty());
    writeResponse(socket, req.header.seq, {{"model", "ci05"}});
    QTRY_VERIFY(called);
}

void IrServiceTest::ci05CommonCommandsUseCi05Namespace()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    IrService service(&client);
    service.ci05SetBrightness(nullptr, 50, nullptr);

    CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.ci05.set_brightness"));
    QCOMPARE(req.payload["params"].value("value", -1), 50);

    service.ci05FocusStartPositive(nullptr, nullptr);
    req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.ci05.focus_start_positive"));
    QVERIFY(req.payload["params"].empty());

    service.ci05FocusStop(nullptr, nullptr);
    req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.ci05.focus_stop"));
    QVERIFY(req.payload["params"].empty());
}

void IrServiceTest::ci05StatusReadReturnsServerData()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    IrService service(&client);
    bool called = false;
    service.ci05ReadStatus1(nullptr, [&called](bool ok, const nlohmann::json& data, const QString& err) {
        called = true;
        QVERIFY2(ok, qPrintable(err));
        const auto value = data.value("value", nlohmann::json());
        QCOMPARE(value.value("brightness", -1), 50);
        QCOMPARE(value.value("integration_us", -1), 1000);
    });

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("ir.ci05.read_status1"));
    QVERIFY(req.payload["params"].empty());
    writeResponse(socket, req.header.seq,
                  {{"value", {{"brightness", 50}, {"integration_us", 1000}}}});
    QTRY_VERIFY(called);
}

QTEST_MAIN(IrServiceTest)

#include "IrServiceTest.moc"

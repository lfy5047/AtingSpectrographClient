#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstring>

#include "ControlClient.h"
#include "Protocol.h"
#include "TempControlService.h"

class TempControlServiceTest : public QObject {
    Q_OBJECT

private slots:
    void setAdjustTemperatureSendsValue();
    void setSwitchSendsEnable();
    void statusParsesServerData();
    void querySendsSelectedKey();
    void setParameterSendsSelectedKeyAndValue();
    void saveSendsSelectedKey();
    void sendRawSendsCommand();
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

}

void TempControlServiceTest::setAdjustTemperatureSendsValue()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    service.setAdjustTemperature(nullptr, 25.5, nullptr);

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.set_adjust_temperature"));
    QCOMPARE(req.payload["params"].value("value", 0.0), 25.5);
}

void TempControlServiceTest::setSwitchSendsEnable()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    service.setSwitch(nullptr, true, nullptr);

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.set_switch"));
    QCOMPARE(req.payload["params"].value("enable", false), true);
}

void TempControlServiceTest::statusParsesServerData()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    bool called = false;
    service.status(nullptr, [&called](bool ok, const TempControlStatus& status, const QString& err) {
        called = true;
        QVERIFY2(ok, qPrintable(err));
        QCOMPARE(status.adjustTemperature, 25.5);
        QCOMPARE(status.actualTemperature, 24.8);
        QCOMPARE(status.actualVoltage, -1.25);
        QCOMPARE(status.switchEnabled, true);
        QCOMPARE(status.outputEnabled, true);
        QCOMPARE(status.errorStatus, QStringLiteral("255"));
        QCOMPARE(status.timestamp, static_cast<qint64>(123456));
    });

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.status"));
    writeResponse(socket, req.header.seq,
                  {{"adjust_temperature", 25.5},
                   {"actual_temperature", 24.8},
                   {"actual_voltage", -1.25},
                   {"switch", 1},
                   {"output_enabled", 1},
                   {"error_status", "255"},
                   {"ts", 123456}});
    QTRY_VERIFY(called);
}

void TempControlServiceTest::querySendsSelectedKey()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    service.query(nullptr, QStringLiteral("actual_temperature"), nullptr);

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.query"));
    QCOMPARE(QString::fromStdString(req.payload["params"].value("key", std::string())),
             QStringLiteral("actual_temperature"));
}

void TempControlServiceTest::setParameterSendsSelectedKeyAndValue()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    service.set(nullptr, QStringLiteral("ramp_speed"), QStringLiteral("1.25"), nullptr);

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.set"));
    QCOMPARE(QString::fromStdString(req.payload["params"].value("key", std::string())),
             QStringLiteral("ramp_speed"));
    QCOMPARE(QString::fromStdString(req.payload["params"].value("value", std::string())),
             QStringLiteral("1.25"));
}

void TempControlServiceTest::saveSendsSelectedKey()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    service.save(nullptr, QStringLiteral("adjust_temperature"), nullptr);

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.save"));
    QCOMPARE(QString::fromStdString(req.payload["params"].value("key", std::string())),
             QStringLiteral("adjust_temperature"));
}

void TempControlServiceTest::sendRawSendsCommand()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    TempControlService service(&client);
    service.sendRaw(nullptr, QStringLiteral("TC1:TCACTTEMP?"), nullptr);

    const CapturedRequest req = readRequest(socket);
    verifyRequestFrame(req, QStringLiteral("tempctrl.send_raw"));
    QCOMPARE(QString::fromStdString(req.payload["params"].value("command", std::string())),
             QStringLiteral("TC1:TCACTTEMP?"));
}

QTEST_MAIN(TempControlServiceTest)

#include "TempControlServiceTest.moc"

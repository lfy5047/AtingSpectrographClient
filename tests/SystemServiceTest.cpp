#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

#include "ControlClient.h"
#include "Protocol.h"
#include "SystemService.h"

class SystemServiceTest : public QObject {
    Q_OBJECT

private slots:
    void backgroundCalibrationStartSendsCommandAndParsesResponse();
    void backgroundCalibrationStatusParsesResponse();
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

}

void SystemServiceTest::backgroundCalibrationStartSendsCommandAndParsesResponse()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    SystemService service(&client);
    bool called = false;
    service.startBackgroundCalibration(nullptr,
        [&called](bool ok, const BackgroundCalibrationStart& result, const QString& err) {
            called = true;
            QVERIFY2(ok, qPrintable(err));
            QVERIFY(result.started);
            QCOMPARE(result.taskId, 7);
            QCOMPARE(result.stage, QStringLiteral("moving_to_background"));
        });

    const CapturedRequest req = readRequest(socket);
    QCOMPARE(QString::fromStdString(req.payload.value("cmd", std::string())),
             QStringLiteral("system.background_calibration.start"));
    QVERIFY(req.payload.contains("params"));
    QVERIFY(req.payload["params"].empty());
    writeResponse(socket, req.header.seq,
                  {{"started", true}, {"task_id", 7}, {"stage", "moving_to_background"}});
    QTRY_VERIFY(called);
}

void SystemServiceTest::backgroundCalibrationStatusParsesResponse()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    SystemService service(&client);
    bool called = false;
    service.backgroundCalibrationStatus(nullptr,
        [&called](bool ok, const BackgroundCalibrationStatus& status, const QString& err) {
            called = true;
            QVERIFY2(ok, qPrintable(err));
            QCOMPARE(status.taskId, 7);
            QVERIFY(!status.running);
            QCOMPARE(status.stage, QStringLiteral("failed"));
            QCOMPARE(status.error, QStringLiteral("mirror timeout"));
            QCOMPARE(status.startedAtMs, static_cast<qint64>(100));
            QCOMPARE(status.finishedAtMs, static_cast<qint64>(200));
        });

    const CapturedRequest req = readRequest(socket);
    QCOMPARE(QString::fromStdString(req.payload.value("cmd", std::string())),
             QStringLiteral("system.background_calibration.status"));
    QVERIFY(req.payload.contains("params"));
    QVERIFY(req.payload["params"].empty());
    writeResponse(socket, req.header.seq,
                  {{"task_id", 7}, {"running", false}, {"stage", "failed"},
                   {"error", "mirror timeout"}, {"started_at_ms", 100}, {"finished_at_ms", 200}});
    QTRY_VERIFY(called);
}

QTEST_MAIN(SystemServiceTest)

#include "SystemServiceTest.moc"

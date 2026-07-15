#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

#include "ControlClient.h"
#include "Protocol.h"
#include "RpcTypes.h"
#include "SystemService.h"

class SystemServiceTest : public QObject {
    Q_OBJECT

private slots:
    void streamProtocolUsesV4ColumnNucChannels();
    void backgroundCalibrationStartSendsCommandAndParsesResponse();
    void backgroundCalibrationStatusParsesResponse();
    void columnNucConfigCommandsUseProtocol();
    void columnNucCaptureCommandsUseProtocolAndPreserveErrors();
    void columnNucCalibrationUsesExplicitInputs();
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

void writeErrorResponse(QTcpSocket* socket, uint32_t seq, int code, const QString& message)
{
    using namespace cli::proto;

    const std::string body = nlohmann::json{
        {"ok", false}, {"code", code}, {"msg", message.toStdString()}
    }.dump();
    QByteArray frame(static_cast<int>(sizeof(CtrlHeader) + body.size()), '\0');

    CtrlHeader header;
    header.magic = kCtrlMagic;
    header.version = kCtrlProtoVersion;
    header.type = static_cast<uint16_t>(Error);
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

void SystemServiceTest::streamProtocolUsesV4ColumnNucChannels()
{
    QCOMPARE(cli::proto::kStreamProtoVersion, static_cast<uint8_t>(4));
    QCOMPARE(cli::proto::kStreamMetadataProtoVersion, static_cast<uint8_t>(3));
    QCOMPARE(static_cast<int>(cli::proto::NucRaw16), 5);
    QCOMPARE(static_cast<int>(cli::proto::SpectralPreview), 6);
    QCOMPARE(cli::proto::channelBit(cli::proto::NucRaw16), static_cast<uint32_t>(1u << 5));
    QCOMPARE(cli::proto::channelBit(cli::proto::SpectralPreview), static_cast<uint32_t>(1u << 6));
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

void SystemServiceTest::columnNucConfigCommandsUseProtocol()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());
    SystemService service(&client);

    bool getCalled = false;
    service.columnNucGetConfig(nullptr, [&getCalled](const RpcResult& result) {
        getCalled = true;
        QVERIFY(result.ok);
        QVERIFY(result.data.value("matrices_loaded", false));
    });
    CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.get_config"));
    QVERIFY(request.payload["params"].empty());
    writeResponse(socket, request.header.seq, {{"matrices_loaded", true}});
    QTRY_VERIFY(getCalled);

    bool setCalled = false;
    service.columnNucSetConfig(nullptr, true, QStringLiteral("gain.raw"),
                               QStringLiteral("offset.raw"), 380, 512, 1e-6,
                               [&setCalled](const RpcResult& result) {
        setCalled = true;
        QVERIFY(result.ok);
    });
    request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.set_config"));
    const auto& setParams = request.payload["params"];
    QVERIFY(setParams.value("enabled", false));
    QCOMPARE(QString::fromStdString(setParams.value("gain_file", std::string())), QStringLiteral("gain.raw"));
    QCOMPARE(QString::fromStdString(setParams.value("offset_file", std::string())), QStringLiteral("offset.raw"));
    QCOMPARE(setParams.value("width", 0), 380);
    QCOMPARE(setParams.value("height", 0), 512);
    QCOMPARE(setParams.value("eps", 0.0), 1e-6);
    writeResponse(socket, request.header.seq, {{"enabled", true}});
    QTRY_VERIFY(setCalled);

    bool reloadCalled = false;
    service.columnNucReload(nullptr, [&reloadCalled](const RpcResult& result) {
        reloadCalled = true;
        QVERIFY(result.ok);
    });
    request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.reload"));
    QVERIFY(request.payload["params"].empty());
    writeResponse(socket, request.header.seq, {{"matrices_loaded", true}});
    QTRY_VERIFY(reloadCalled);
}

void SystemServiceTest::columnNucCaptureCommandsUseProtocolAndPreserveErrors()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());
    SystemService service(&client);

    bool captureCalled = false;
    service.columnNucCapture(nullptr, QStringLiteral("low"), 30.0, 64, 10000,
                             [&captureCalled](const RpcResult& result) {
        captureCalled = true;
        QVERIFY(result.ok);
        QCOMPARE(QString::fromStdString(result.data.value("task_id", std::string())),
                 QStringLiteral("task_low"));
    });
    CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.capture"));
    const auto& captureParams = request.payload["params"];
    QCOMPARE(QString::fromStdString(captureParams.value("level", std::string())), QStringLiteral("low"));
    QCOMPARE(captureParams.value("temperature", 0.0), 30.0);
    QCOMPARE(captureParams.value("frame_count", 0), 64);
    QCOMPARE(captureParams.value("timeout_ms", 0), 10000);
    writeResponse(socket, request.header.seq, {{"task_id", "task_low"}});
    QTRY_VERIFY(captureCalled);

    bool statusCalled = false;
    service.columnNucCaptureStatus(nullptr, QStringLiteral("task_low"),
                                   [&statusCalled](const RpcResult& result) {
        statusCalled = true;
        QVERIFY(!result.ok);
        QCOMPARE(result.code, -12);
        QCOMPARE(result.msg, QStringLiteral("capture busy"));
    });
    request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.capture_status"));
    QCOMPARE(QString::fromStdString(request.payload["params"].value("task_id", std::string())),
             QStringLiteral("task_low"));
    writeErrorResponse(socket, request.header.seq, -12, QStringLiteral("capture busy"));
    QTRY_VERIFY(statusCalled);

    bool cancelCalled = false;
    service.columnNucCaptureCancel(nullptr, QStringLiteral("task_low"),
                                   [&cancelCalled](const RpcResult& result) {
        cancelCalled = true;
        QVERIFY(result.ok);
    });
    request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.capture_cancel"));
    QCOMPARE(QString::fromStdString(request.payload["params"].value("task_id", std::string())),
             QStringLiteral("task_low"));
    writeResponse(socket, request.header.seq, {{"state", "cancelled"}});
    QTRY_VERIFY(cancelCalled);

    bool listCalled = false;
    service.columnNucListCaptures(nullptr, 100, [&listCalled](const RpcResult& result) {
        listCalled = true;
        QVERIFY(result.ok);
    });
    request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.list_captures"));
    QCOMPARE(request.payload["params"].value("count", 0), 100);
    writeResponse(socket, request.header.seq, {{"low", nlohmann::json::array()},
                                                {"high", nlohmann::json::array()}});
    QTRY_VERIFY(listCalled);
}

void SystemServiceTest::columnNucCalibrationUsesExplicitInputs()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());
    SystemService service(&client);

    bool called = false;
    service.columnNucCalibrate(nullptr, QStringLiteral("low.raw"), QStringLiteral("high.raw"),
                               QStringLiteral("./matrix"), 380, 512, 1e-6, true,
                               [&called](const RpcResult& result) {
        called = true;
        QVERIFY(result.ok);
        QVERIFY(result.data.value("applied", false));
    });
    const CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("system.column_nuc.calibrate"));
    const auto& params = request.payload["params"];
    QCOMPARE(QString::fromStdString(params.value("low_file", std::string())), QStringLiteral("low.raw"));
    QCOMPARE(QString::fromStdString(params.value("high_file", std::string())), QStringLiteral("high.raw"));
    QCOMPARE(QString::fromStdString(params.value("out_dir", std::string())), QStringLiteral("./matrix"));
    QCOMPARE(params.value("width", 0), 380);
    QCOMPARE(params.value("height", 0), 512);
    QCOMPARE(params.value("eps", 0.0), 1e-6);
    QVERIFY(params.value("apply", false));
    writeResponse(socket, request.header.seq, {{"applied", true}});
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

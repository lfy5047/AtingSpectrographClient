#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <QCoreApplication>
#include <QElapsedTimer>

#include <cstring>

#include "BinningService.h"
#include "ControlClient.h"
#include "Protocol.h"

class BinningServiceTest : public QObject {
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

void BinningServiceTest::getConfigParsesServerData()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    BinningService service(&client);
    bool called = false;
    service.getConfig(nullptr, [&called](bool ok, const BinningConfig& config, const QString& err) {
        called = true;
        QVERIFY2(ok, qPrintable(err));
        QVERIFY(config.enabled);
        QCOMPARE(config.spectralFactor, 2);
        QCOMPARE(config.spatialFactor, 4);
    });

    const CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("binning.get_config"));
    writeResponse(socket, request.header.seq,
                  {{"enabled", true}, {"spectral_factor", 2}, {"spatial_factor", 4}});
    QTRY_VERIFY(called);
}

void BinningServiceTest::setConfigSendsAllFields()
{
    QTcpServer server;
    ControlClient client;
    QTcpSocket* socket = connectClient(server, client);
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    BinningService service(&client);
    BinningConfig config;
    config.enabled = true;
    config.spectralFactor = 4;
    config.spatialFactor = 4;
    service.setConfig(nullptr, config, nullptr);

    const CapturedRequest request = readRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("binning.set_config"));
    QVERIFY(request.payload.contains("params"));
    QVERIFY(request.payload["params"].value("enabled", false));
    QCOMPARE(request.payload["params"].value("spectral_factor", 0), 4);
    QCOMPARE(request.payload["params"].value("spatial_factor", 0), 4);
}

QTEST_MAIN(BinningServiceTest)

#include "BinningServiceTest.moc"

#include <QtTest/QtTest>

#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

#include "ControlClient.h"
#include "Protocol.h"
#include "RecordService.h"

class RecordServiceTest : public QObject {
    Q_OBJECT

private slots:
    void removeRecordsSendsDeleteRequest();
};

void RecordServiceTest::removeRecordsSendsDeleteRequest()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    ControlClient client;
    client.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());

    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(client.isConnected());

    RecordService service(&client);
    service.removeRecords(nullptr, QStringLiteral("raw"),
                          {QStringLiteral("1750972329960213883"),
                           QStringLiteral("1750972329960213884")},
                          nullptr);

    using namespace cli::proto;
    QTRY_VERIFY(socket->bytesAvailable() >= static_cast<qint64>(sizeof(CtrlHeader)));

    CtrlHeader hdr;
    std::memcpy(&hdr, socket->read(sizeof(CtrlHeader)).constData(), sizeof(CtrlHeader));
    QCOMPARE(hdr.magic, kCtrlMagic);
    QCOMPARE(hdr.version, kCtrlProtoVersion);
    QCOMPARE(hdr.type, static_cast<uint16_t>(Request));
    QVERIFY(hdr.payload_len > 0);

    while (socket->bytesAvailable() < hdr.payload_len) {
        QVERIFY(socket->waitForReadyRead(1000));
    }
    const QByteArray payloadBytes = socket->read(hdr.payload_len);
    const nlohmann::json payload = nlohmann::json::parse(payloadBytes.constData());

    QCOMPARE(QString::fromStdString(payload.value("cmd", std::string())),
             QStringLiteral("record.delete"));
    QVERIFY(payload.contains("params"));
    QCOMPARE(QString::fromStdString(payload["params"].value("type", std::string())),
             QStringLiteral("raw"));
    QVERIFY(payload["params"].contains("items"));
    QVERIFY(payload["params"]["items"].is_array());
    QCOMPARE(payload["params"]["items"].size(), 2u);
    QCOMPARE(QString::fromStdString(payload["params"]["items"][0].get<std::string>()),
             QStringLiteral("1750972329960213883"));
    QCOMPARE(QString::fromStdString(payload["params"]["items"][1].get<std::string>()),
             QStringLiteral("1750972329960213884"));
}

QTEST_MAIN(RecordServiceTest)

#include "RecordServiceTest.moc"

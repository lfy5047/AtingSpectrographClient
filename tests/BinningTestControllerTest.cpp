#include <QtTest/QtTest>

#include "DeviceClient.h"
#include "Protocol.h"
#include "Ui/BinningTestController.h"
#include "Ui/panels/BinningTestPanel.h"
#include "Ui/widgets/BinningCompareWidget.h"
#include "Ui/widgets/ImageView.h"

#include <QCoreApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstring>

class BinningTestControllerTest : public QObject {
    Q_OBJECT

private slots:
    void capturesThreeFactorsAndRestoresOriginalConfig_data();
    void capturesThreeFactorsAndRestoresOriginalConfig();
};

namespace {

struct CapturedRequest {
    cli::proto::CtrlHeader header;
    nlohmann::json payload;
};

bool readRequest(QTcpSocket* socket, CapturedRequest* out)
{
    using namespace cli::proto;
    if (!socket || !out) return false;
    CapturedRequest request;
    QElapsedTimer timer;
    timer.start();
    while (socket->bytesAvailable() < static_cast<qint64>(sizeof(CtrlHeader))
           && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        socket->waitForReadyRead(20);
    }
    if (socket->bytesAvailable() < static_cast<qint64>(sizeof(CtrlHeader))) return false;

    const QByteArray headerBytes = socket->read(sizeof(CtrlHeader));
    if (headerBytes.size() != static_cast<int>(sizeof(CtrlHeader))) return false;
    std::memcpy(&request.header, headerBytes.constData(), sizeof(CtrlHeader));
    timer.restart();
    while (socket->bytesAvailable() < request.header.payload_len && timer.elapsed() < 2000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        socket->waitForReadyRead(20);
    }
    if (socket->bytesAvailable() < request.header.payload_len) return false;
    try {
        request.payload = nlohmann::json::parse(socket->read(request.header.payload_len).constData());
    } catch (...) {
        return false;
    }
    *out = request;
    return true;
}

bool writeConfigResponse(QTcpSocket* socket, uint32_t seq, int factor, bool enabled)
{
    using namespace cli::proto;
    const nlohmann::json data = {
        {"enabled", enabled},
        {"spectral_factor", factor},
        {"spatial_factor", factor},
    };
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
    if (!socket->waitForBytesWritten(1000)) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return true;
}

bool writeFailureResponse(QTcpSocket* socket, uint32_t seq, const QString& message)
{
    using namespace cli::proto;
    const std::string body = nlohmann::json{
        {"ok", false},
        {"code", -1},
        {"msg", message.toStdString()},
    }.dump();
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
    if (!socket->waitForBytesWritten(1000)) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return true;
}

bool answerConfigRequest(QTcpSocket* socket, const QString& expectedCommand,
                         int factor, bool enabled)
{
    CapturedRequest request;
    if (!readRequest(socket, &request)) return false;
    if (QString::fromStdString(request.payload.value("cmd", std::string())) != expectedCommand) {
        return false;
    }
    if (expectedCommand == QStringLiteral("binning.set_config")) {
        if (!request.payload.contains("params")
            || request.payload["params"].value("spectral_factor", 0) != factor
            || request.payload["params"].value("spatial_factor", 0) != factor
            || request.payload["params"].value("enabled", !enabled) != enabled) {
            return false;
        }
    }
    return writeConfigResponse(socket, request.header.seq, factor, enabled);
}

bool answerFailedRequest(QTcpSocket* socket, const QString& expectedCommand)
{
    CapturedRequest request;
    if (!readRequest(socket, &request)) return false;
    if (QString::fromStdString(request.payload.value("cmd", std::string())) != expectedCommand) {
        return false;
    }
    return writeFailureResponse(socket, request.header.seq, QStringLiteral("restore failed"));
}

void sendStableFrames(DeviceClient& device, int channel,
                      int width, int height, quint64& frameId)
{
    for (int i = 0; i < 3; ++i) {
        StreamFrame frame;
        frame.channel = channel;
        frame.width = width;
        frame.height = height;
        frame.pixfmt = cli::proto::Mono16;
        frame.streamFrameId = ++frameId;
        frame.data.resize(width * height * 2);
        auto* pixels = reinterpret_cast<quint16*>(frame.data.data());
        for (int pixel = 0; pixel < width * height; ++pixel) {
            pixels[pixel] = static_cast<quint16>(100 + i + pixel);
        }
        emit device.frameReady(frame);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

} // namespace

void BinningTestControllerTest::capturesThreeFactorsAndRestoresOriginalConfig_data()
{
    QTest::addColumn<int>("sourceChannel");
    QTest::newRow("raw16") << static_cast<int>(cli::proto::Raw16);
    QTest::newRow("slice-stitch16") << static_cast<int>(cli::proto::SliceStitch16);
}

void BinningTestControllerTest::capturesThreeFactorsAndRestoresOriginalConfig()
{
    QFETCH(int, sourceChannel);

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    BinningTestPanel panel;
    BinningCompareWidget compare;
    BinningTestController controller(&device, &panel, &compare);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    QVERIFY2(answerConfigRequest(socket, QStringLiteral("binning.get_config"), 1, false),
             "initial config refresh did not complete");

    auto* source = panel.findChild<QComboBox*>(QStringLiteral("binningSourceCombo"));
    QVERIFY(source);
    const int sourceIndex = source->findData(sourceChannel);
    QVERIFY(sourceIndex >= 0);
    source->setCurrentIndex(sourceIndex);

    auto* start = panel.findChild<QPushButton*>(QStringLiteral("binningStartTestButton"));
    QVERIFY(start);
    start->click();
    QVERIFY2(answerConfigRequest(socket, QStringLiteral("binning.get_config"), 1, false),
             "test did not request original config");

    quint64 frameId = 0;
    const int factors[] = {1, 2, 4};
    const int widths[] = {8, 4, 2};
    const int heights[] = {8, 4, 2};
    for (int index = 0; index < 3; ++index) {
        const int factor = factors[index];
        QVERIFY2(answerConfigRequest(socket, QStringLiteral("binning.set_config"), factor, factor != 1),
                 qPrintable(QStringLiteral("missing set_config for factor %1").arg(factor)));
        QVERIFY2(answerConfigRequest(socket, QStringLiteral("binning.get_config"), factor, factor != 1),
                 qPrintable(QStringLiteral("missing get_config for factor %1").arg(factor)));
        sendStableFrames(device, sourceChannel, widths[index], heights[index], frameId);
    }

    QVERIFY2(answerFailedRequest(socket, QStringLiteral("binning.set_config")),
             "test did not request original config restoration");
    QTRY_VERIFY(!controller.prepareForClose());

    start->click();
    auto* apply = panel.findChild<QPushButton*>(QStringLiteral("binningApplyFactorButton"));
    QVERIFY(apply);
    apply->click();
    QTest::qWait(50);
    QCOMPARE(socket->bytesAvailable(), qint64(0));

    auto* refresh = panel.findChild<QPushButton*>(QStringLiteral("binningRefreshButton"));
    QVERIFY(refresh);
    refresh->click();
    QVERIFY2(answerConfigRequest(socket, QStringLiteral("binning.set_config"), 1, false),
             "refresh did not retry original config restoration");
    QTRY_VERIFY(controller.prepareForClose());
    QTRY_VERIFY(start->isEnabled());

    QCOMPARE(compare.imageViewForFactor(1)->currentImage().size(), QSize(8, 8));
    QCOMPARE(compare.imageViewForFactor(2)->currentImage().size(), QSize(4, 4));
    QCOMPARE(compare.imageViewForFactor(4)->currentImage().size(), QSize(2, 2));
}

QTEST_MAIN(BinningTestControllerTest)

#include "BinningTestControllerTest.moc"

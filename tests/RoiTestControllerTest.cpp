#include <QtTest/QtTest>

#include "DeviceClient.h"
#include "Protocol.h"
#include "Ui/RoiTestController.h"
#include "Ui/panels/RoiTestPanel.h"
#include "Ui/widgets/ImageView.h"
#include "Ui/widgets/RoiCompareWidget.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <cstring>
#include <functional>

class RoiTestControllerTest : public QObject {
    Q_OBJECT

private slots:
    void capturesFullAndWindowedFramesAndRestoresConfigs();
    void capturesOnlyFullFrameWhenWindowingDisabled();
    void rejectsNonUnitBinning();
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

bool writeResponse(QTcpSocket* socket, uint32_t seq, const nlohmann::json& data)
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
    if (!socket->waitForBytesWritten(1000)) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return true;
}

bool answerRequest(QTcpSocket* socket, const QString& expectedCommand,
                   const nlohmann::json& responseData,
                   const std::function<bool(const nlohmann::json&)>& validate = {})
{
    CapturedRequest request;
    if (!readRequest(socket, &request)) return false;
    if (QString::fromStdString(request.payload.value("cmd", std::string())) != expectedCommand) {
        return false;
    }
    if (validate && !validate(request.payload.value("params", nlohmann::json::object()))) {
        return false;
    }
    return writeResponse(socket, request.header.seq, responseData);
}

nlohmann::json roiData(int begin, int end, int hBegin, int hEnd,
                       bool collecting = false, bool pending = false)
{
    return {
        {"slice_begin", begin},
        {"slice_end", end},
        {"slice_h_begin", hBegin},
        {"slice_h_end", hEnd},
        {"is_collecting", collecting},
        {"pending_apply", pending},
    };
}

nlohmann::json gateData(bool staticMode, bool collecting = false, bool pending = false)
{
    return {
        {"discard_front_ms", 11},
        {"discard_back_ms", 22},
        {"forward_offset_frames", -3},
        {"reverse_offset_frames", 4},
        {"static_collect_mode", staticMode},
        {"is_collecting", collecting},
        {"pending_config", pending},
    };
}

bool matchesRoi(const nlohmann::json& params,
                int begin, int end, int hBegin, int hEnd)
{
    return params.value("slice_begin", -1) == begin
        && params.value("slice_end", -1) == end
        && params.value("slice_h_begin", -1) == hBegin
        && params.value("slice_h_end", -1) == hEnd;
}

bool matchesGate(const nlohmann::json& params, bool staticMode)
{
    return params.value("discard_front_ms", -1) == 11
        && params.value("discard_back_ms", -1) == 22
        && params.value("forward_offset_frames", 0) == -3
        && params.value("reverse_offset_frames", 0) == 4
        && params.value("static_collect_mode", !staticMode) == staticMode;
}

void sendFrame(DeviceClient& device, int width, int height,
               quint8 frameType, quint64 frameId)
{
    StreamFrame frame;
    frame.channel = cli::proto::SliceStitch16;
    frame.width = width;
    frame.height = height;
    frame.pixfmt = cli::proto::Mono16;
    frame.frameType = frameType;
    frame.streamFrameId = frameId;
    frame.data.resize(width * height * 2);
    auto* pixels = reinterpret_cast<quint16*>(frame.data.data());
    for (int pixel = 0; pixel < width * height; ++pixel) {
        pixels[pixel] = static_cast<quint16>(100 + (pixel % 1000));
    }
    emit device.frameReady(frame);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

} // namespace

void RoiTestControllerTest::capturesFullAndWindowedFramesAndRestoresConfigs()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    RoiTestPanel panel;
    RoiCompareWidget compare;
    RoiTestController controller(&device, &panel, &compare);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    auto* start = panel.findChild<QPushButton*>(QStringLiteral("roiStartTestButton"));
    QVERIFY(start);
    start->click();

    QVERIFY(answerRequest(socket, QStringLiteral("binning.get_config"),
                          {{"enabled", false}, {"spectral_factor", 1}, {"spatial_factor", 1}}));
    QVERIFY(answerRequest(socket, QStringLiteral("camera.get_resolution"),
                          {{"width", 640}, {"height", 512}}));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.get_config"),
                          roiData(10, 600, 5, 500)));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.get_gate_config"), gateData(false)));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.set_gate_config"), gateData(true),
                          [](const nlohmann::json& params) { return matchesGate(params, true); }));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.set_config"), roiData(0, 640, 0, 512),
                          [](const nlohmann::json& params) {
                              return matchesRoi(params, 0, 640, 0, 512);
                          }));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.get_config"), roiData(0, 640, 0, 512)));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.start"), nlohmann::json::object()));

    sendFrame(device, 640, 512, cli::proto::HeaderFrame, 100);
    QTest::qWait(30);
    QCOMPARE(socket->bytesAvailable(), qint64(0));
    sendFrame(device, 640, 512, cli::proto::DataFrame, 101);

    QVERIFY(answerRequest(socket, QStringLiteral("roi.set_config"),
                          roiData(240, 435, 0, 512, true, true),
                          [](const nlohmann::json& params) {
                              return matchesRoi(params, 240, 435, 0, 512);
                          }));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.get_config"),
                          roiData(240, 435, 0, 512, true, true)));

    sendFrame(device, 195, 512, cli::proto::HeaderFrame, 200);
    QTest::qWait(30);
    QCOMPARE(socket->bytesAvailable(), qint64(0));
    sendFrame(device, 195, 512, cli::proto::DataFrame, 201);

    QVERIFY(answerRequest(socket, QStringLiteral("collect.stop"), nlohmann::json::object()));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.set_config"), roiData(10, 600, 5, 500),
                          [](const nlohmann::json& params) {
                              return matchesRoi(params, 10, 600, 5, 500);
                          }));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.set_gate_config"), gateData(false),
                          [](const nlohmann::json& params) { return matchesGate(params, false); }));

    QTRY_VERIFY(controller.prepareForClose());
    QTRY_VERIFY(start->isEnabled());
    QCOMPARE(compare.fullFrameView()->currentImage().size(), QSize(640, 512));
    QCOMPARE(compare.roiView()->currentImage().size(), QSize(195, 512));
}

void RoiTestControllerTest::capturesOnlyFullFrameWhenWindowingDisabled()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    RoiTestPanel panel;
    RoiCompareWidget compare;
    RoiTestController controller(&device, &panel, &compare);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    auto* applyWindowing = panel.findChild<QCheckBox*>(
        QStringLiteral("roiApplyWindowingCheck"));
    auto* start = panel.findChild<QPushButton*>(QStringLiteral("roiStartTestButton"));
    QVERIFY(applyWindowing);
    QVERIFY(start);
    applyWindowing->setChecked(false);
    start->click();

    QVERIFY(answerRequest(socket, QStringLiteral("binning.get_config"),
                          {{"enabled", false}, {"spectral_factor", 1}, {"spatial_factor", 1}}));
    QVERIFY(answerRequest(socket, QStringLiteral("camera.get_resolution"),
                          {{"width", 640}, {"height", 512}}));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.get_config"),
                          roiData(10, 600, 5, 500)));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.get_gate_config"), gateData(false)));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.set_gate_config"), gateData(true),
                          [](const nlohmann::json& params) { return matchesGate(params, true); }));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.set_config"), roiData(0, 640, 0, 512),
                          [](const nlohmann::json& params) {
                              return matchesRoi(params, 0, 640, 0, 512);
                          }));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.get_config"), roiData(0, 640, 0, 512)));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.start"), nlohmann::json::object()));

    sendFrame(device, 640, 512, cli::proto::DataFrame, 101);

    QVERIFY(answerRequest(socket, QStringLiteral("collect.stop"), nlohmann::json::object()));
    QVERIFY(answerRequest(socket, QStringLiteral("roi.set_config"), roiData(10, 600, 5, 500),
                          [](const nlohmann::json& params) {
                              return matchesRoi(params, 10, 600, 5, 500);
                          }));
    QVERIFY(answerRequest(socket, QStringLiteral("collect.set_gate_config"), gateData(false),
                          [](const nlohmann::json& params) { return matchesGate(params, false); }));

    QTRY_VERIFY(controller.prepareForClose());
    QTRY_VERIFY(start->isEnabled());
    QCOMPARE(compare.fullFrameView()->currentImage().size(), QSize(640, 512));
    QVERIFY(compare.roiView()->currentImage().isNull());
}

void RoiTestControllerTest::rejectsNonUnitBinning()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    RoiTestPanel panel;
    RoiCompareWidget compare;
    RoiTestController controller(&device, &panel, &compare);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    auto* start = panel.findChild<QPushButton*>(QStringLiteral("roiStartTestButton"));
    auto* status = panel.findChild<QLabel*>(QStringLiteral("roiStatusLabel"));
    QVERIFY(start);
    QVERIFY(status);
    start->click();
    QVERIFY(answerRequest(socket, QStringLiteral("binning.get_config"),
                          {{"enabled", true}, {"spectral_factor", 2}, {"spatial_factor", 2}}));

    QTRY_VERIFY(start->isEnabled());
    QTRY_VERIFY(status->text().contains(QStringLiteral("Binning")));
    QVERIFY(controller.prepareForClose());
}

QTEST_MAIN(RoiTestControllerTest)

#include "RoiTestControllerTest.moc"

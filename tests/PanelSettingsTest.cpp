#include <QtTest/QtTest>

#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>

#include <algorithm>
#include <cstring>

#include "CameraPanel.h"
#include "CollectPanel.h"
#include "ConnectionPanel.h"
#include "DeviceClient.h"
#include "IrPanel.h"
#include "MirrorPanel.h"
#include "PanelSettings.h"
#include "Protocol.h"
#include "SpectralPanel.h"
#include "StreamPanel.h"
#include "TempControlPanel.h"

class PanelSettingsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void restoresComboByItemData();
    void fallsBackWhenComboDataIsMissing();
    void returnsCurrentComboData();
    void connectionPanelRestoresSavedEndpoint();
    void cameraPanelRestoresSavedResolution();
    void mirrorPanelRestoresSavedMotionInputs();
    void collectPanelRestoresSavedInputs();
    void collectPanelProvidesBackgroundCalibrationControls();
    void tempControlPanelRestoresSavedUserInputs();
    void tempControlStatusRefreshKeepsTargetInput();
    void tempControlCommonSaveButtonsSendSaveCommands();
    void streamPanelRestoresSavedChannels();
    void spectralPanelRestoresSavedRenderInputs();
    void spectralPanelShowsSavedBandsBeforeStreamMetadata();
    void spectralPanelKeepsSavedBandsWhenStatsAreEmpty();
    void irPanelRestoresSavedUserInputs();
    void irPanelSwitchesBetweenSeparateLegacyAndCi05Pages();
    void irPanelCi05PageUsesSeparateCi05Controls();
    void irPanelCi05PageReadsAllStatusRegisters();
    void irPanelCi05PageTriggersAllCompensations();
};

void PanelSettingsTest::initTestCase()
{
    const QString settingsRoot = QDir::tempPath() + QStringLiteral("/AtingSpectrographClientPanelSettingsTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot);
    QCoreApplication::setOrganizationName(QStringLiteral("AtingSpectrographTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PanelSettingsTest"));
}

void PanelSettingsTest::init()
{
    QSettings().clear();
}

namespace {

struct CapturedControlRequest {
    cli::proto::CtrlHeader header;
    nlohmann::json payload;
};

CapturedControlRequest readControlRequest(QTcpSocket* socket)
{
    using namespace cli::proto;

    const int headerSize = static_cast<int>(sizeof(CtrlHeader));
    QElapsedTimer timer;
    timer.start();
    while (socket->bytesAvailable() < headerSize && timer.elapsed() < 1000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        socket->waitForReadyRead(50);
    }
    if (socket->bytesAvailable() < headerSize) return CapturedControlRequest();

    CapturedControlRequest req;
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

void writeControlResponse(QTcpSocket* socket, uint32_t seq, const nlohmann::json& data)
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

bool panelHasLabelText(QWidget* panel, const QString& text)
{
    const auto labels = panel->findChildren<QLabel*>();
    return std::any_of(labels.begin(), labels.end(), [&text](const QLabel* label) {
        return label->text() == text;
    });
}

}

void PanelSettingsTest::restoresComboByItemData()
{
    QComboBox combo;
    combo.addItem(QStringLiteral("A"), QStringLiteral("a"));
    combo.addItem(QStringLiteral("B"), QStringLiteral("b"));

    QVERIFY(PanelSettings::setComboByData(&combo, QStringLiteral("b"), 0));

    QCOMPARE(combo.currentIndex(), 1);
    QCOMPARE(combo.currentData().toString(), QStringLiteral("b"));
}

void PanelSettingsTest::fallsBackWhenComboDataIsMissing()
{
    QComboBox combo;
    combo.addItem(QStringLiteral("A"), QStringLiteral("a"));
    combo.addItem(QStringLiteral("B"), QStringLiteral("b"));

    QVERIFY(!PanelSettings::setComboByData(&combo, QStringLiteral("missing"), 1));

    QCOMPARE(combo.currentIndex(), 1);
    QCOMPARE(combo.currentData().toString(), QStringLiteral("b"));
}

void PanelSettingsTest::returnsCurrentComboData()
{
    QComboBox combo;
    combo.addItem(QStringLiteral("A"), 10);
    combo.addItem(QStringLiteral("B"), 20);
    combo.setCurrentIndex(1);

    QCOMPARE(PanelSettings::comboData(&combo).toInt(), 20);
}

void PanelSettingsTest::connectionPanelRestoresSavedEndpoint()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/connection/host"), QStringLiteral("10.0.0.42"));
    settings.setValue(QStringLiteral("panels/connection/tcpPort"), 9100);
    settings.setValue(QStringLiteral("panels/connection/udpPort"), 1500);

    DeviceClient device;
    ConnectionPanel panel(&device);

    auto* host = panel.findChild<QLineEdit*>(QStringLiteral("connectionHostEdit"));
    auto* tcp = panel.findChild<QSpinBox*>(QStringLiteral("connectionTcpPortSpin"));
    auto* udp = panel.findChild<QSpinBox*>(QStringLiteral("connectionUdpPortSpin"));

    QVERIFY(host);
    QVERIFY(tcp);
    QVERIFY(udp);
    QCOMPARE(host->text(), QStringLiteral("10.0.0.42"));
    QCOMPARE(tcp->value(), 9100);
    QCOMPARE(udp->value(), 1500);
}

void PanelSettingsTest::cameraPanelRestoresSavedResolution()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/camera/width"), 2048);
    settings.setValue(QStringLiteral("panels/camera/height"), 1536);

    DeviceClient device;
    CameraPanel panel(&device);

    auto* width = panel.findChild<QSpinBox*>(QStringLiteral("cameraWidthSpin"));
    auto* height = panel.findChild<QSpinBox*>(QStringLiteral("cameraHeightSpin"));

    QVERIFY(width);
    QVERIFY(height);
    QCOMPARE(width->value(), 2048);
    QCOMPARE(height->value(), 1536);
}

void PanelSettingsTest::mirrorPanelRestoresSavedMotionInputs()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/mirror/targetAngle"), 12.5);
    settings.setValue(QStringLiteral("panels/mirror/sSpeed"), 321);
    settings.setValue(QStringLiteral("panels/mirror/fSpeed"), 654);
    settings.setValue(QStringLiteral("panels/mirror/preset"), 7);

    DeviceClient device;
    MirrorPanel panel(&device);

    auto* target = panel.findChild<QDoubleSpinBox*>(QStringLiteral("mirrorTargetSpin"));
    auto* sSpeed = panel.findChild<QSpinBox*>(QStringLiteral("mirrorSSpeedSpin"));
    auto* fSpeed = panel.findChild<QSpinBox*>(QStringLiteral("mirrorFSpeedSpin"));
    auto* preset = panel.findChild<QComboBox*>(QStringLiteral("mirrorPresetCombo"));

    QVERIFY(target);
    QVERIFY(sSpeed);
    QVERIFY(fSpeed);
    QVERIFY(preset);
    QCOMPARE(target->value(), 12.5);
    QCOMPARE(sSpeed->value(), 321);
    QCOMPARE(fSpeed->value(), 654);
    QCOMPARE(preset->currentData().toInt(), 7);
}

void PanelSettingsTest::collectPanelRestoresSavedInputs()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/collect/oversampleFactor"), 8);
    settings.setValue(QStringLiteral("panels/collect/discardFrontMs"), 11);
    settings.setValue(QStringLiteral("panels/collect/discardBackMs"), 22);
    settings.setValue(QStringLiteral("panels/collect/forwardOffsetFrames"), -3);
    settings.setValue(QStringLiteral("panels/collect/reverseOffsetFrames"), 4);

    DeviceClient device;
    CollectPanel panel(&device);

    auto* oversample = panel.findChild<QSpinBox*>(QStringLiteral("collectOversampleFactorSpin"));
    auto* front = panel.findChild<QSpinBox*>(QStringLiteral("collectDiscardFrontMsSpin"));
    auto* back = panel.findChild<QSpinBox*>(QStringLiteral("collectDiscardBackMsSpin"));
    auto* forward = panel.findChild<QSpinBox*>(QStringLiteral("collectForwardOffsetFramesSpin"));
    auto* reverse = panel.findChild<QSpinBox*>(QStringLiteral("collectReverseOffsetFramesSpin"));

    QVERIFY(oversample);
    QVERIFY(front);
    QVERIFY(back);
    QVERIFY(forward);
    QVERIFY(reverse);
    QCOMPARE(oversample->value(), 8);
    QCOMPARE(front->value(), 11);
    QCOMPARE(back->value(), 22);
    QCOMPARE(forward->value(), -3);
    QCOMPARE(reverse->value(), 4);
}

void PanelSettingsTest::tempControlPanelRestoresSavedUserInputs()
{
    QSettings settings;
    const QString p = QStringLiteral("panels/tempControl/");
    settings.setValue(p + QStringLiteral("targetTemperature"), 26.5);
    settings.setValue(p + QStringLiteral("maxTemperature"), 80.5);
    settings.setValue(p + QStringLiteral("maxVoltage"), 12.5);
    settings.setValue(p + QStringLiteral("selectedKey"), QStringLiteral("ramp_speed"));
    settings.setValue(p + QStringLiteral("advancedValue"), QStringLiteral("1.25"));
    settings.setValue(p + QStringLiteral("module"), QStringLiteral("TC2"));
    settings.setValue(p + QStringLiteral("param"), QStringLiteral("TCPIDP"));
    settings.setValue(p + QStringLiteral("rawCommand"), QStringLiteral("TC1:TCACTTEMP?"));

    DeviceClient device;
    TempControlPanel panel(&device);

    auto* target = panel.findChild<QDoubleSpinBox*>(QStringLiteral("tempControlTargetSpin"));
    auto* maxTemperature = panel.findChild<QDoubleSpinBox*>(QStringLiteral("tempControlMaxTemperatureSpin"));
    auto* maxVoltage = panel.findChild<QDoubleSpinBox*>(QStringLiteral("tempControlMaxVoltageSpin"));
    auto* key = panel.findChild<QComboBox*>(QStringLiteral("tempControlKeyCombo"));
    auto* value = panel.findChild<QLineEdit*>(QStringLiteral("tempControlAdvancedValueEdit"));
    auto* module = panel.findChild<QLineEdit*>(QStringLiteral("tempControlModuleEdit"));
    auto* param = panel.findChild<QLineEdit*>(QStringLiteral("tempControlParamEdit"));
    auto* raw = panel.findChild<QLineEdit*>(QStringLiteral("tempControlRawCommandEdit"));

    QVERIFY(target);
    QVERIFY(maxTemperature);
    QVERIFY(maxVoltage);
    QVERIFY(key);
    QVERIFY(value);
    QVERIFY(!module);
    QVERIFY(!param);
    QVERIFY(!raw);
    QCOMPARE(target->value(), 26.5);
    QCOMPARE(maxTemperature->value(), 80.5);
    QCOMPARE(maxVoltage->value(), 12.5);
    QCOMPARE(key->currentData().toString(), QStringLiteral("ramp_speed"));
    QCOMPARE(value->text(), QStringLiteral("1.25"));
}

void PanelSettingsTest::tempControlStatusRefreshKeepsTargetInput()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    TempControlPanel panel(&device);
    auto* target = panel.findChild<QDoubleSpinBox*>(QStringLiteral("tempControlTargetSpin"));
    QVERIFY(target);
    target->setValue(30.0);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    const CapturedControlRequest req = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(req.payload.value("cmd", std::string())),
             QStringLiteral("tempctrl.status"));
    writeControlResponse(socket, req.header.seq,
                         {{"adjust_temperature", 25.5},
                          {"actual_temperature", 24.8},
                          {"switch", 1},
                          {"output_enabled", 1},
                          {"error_status", "255"},
                          {"ts", 123456}});

    const CapturedControlRequest voltageReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(voltageReq.payload.value("cmd", std::string())),
             QStringLiteral("tempctrl.query"));
    QCOMPARE(QString::fromStdString(voltageReq.payload["params"].value("key", std::string())),
             QStringLiteral("actual_voltage"));
    writeControlResponse(socket, voltageReq.header.seq,
                         {{"key", "actual_voltage"},
                          {"typed_value", -1.25},
                          {"value", "-1.25"}});

    QTRY_VERIFY(panelHasLabelText(&panel, QString::fromUtf8("24.80 ℃")));
    QTRY_VERIFY(panelHasLabelText(&panel, QString::fromUtf8("-1.25 V")));
    QCOMPARE(target->value(), 30.0);
}

void PanelSettingsTest::tempControlCommonSaveButtonsSendSaveCommands()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    TempControlPanel panel(&device);
    auto* saveMaxTemperature = panel.findChild<QPushButton*>(QStringLiteral("tempControlSaveMaxTemperatureButton"));
    auto* saveMaxVoltage = panel.findChild<QPushButton*>(QStringLiteral("tempControlSaveMaxVoltageButton"));
    QVERIFY(saveMaxTemperature);
    QVERIFY(saveMaxVoltage);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    const CapturedControlRequest statusReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(statusReq.payload.value("cmd", std::string())),
             QStringLiteral("tempctrl.status"));

    saveMaxTemperature->click();
    const CapturedControlRequest saveMaxTemperatureReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(saveMaxTemperatureReq.payload.value("cmd", std::string())),
             QStringLiteral("tempctrl.save"));
    QCOMPARE(QString::fromStdString(saveMaxTemperatureReq.payload["params"].value("key", std::string())),
             QStringLiteral("max_temperature"));

    saveMaxVoltage->click();
    const CapturedControlRequest saveMaxVoltageReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(saveMaxVoltageReq.payload.value("cmd", std::string())),
             QStringLiteral("tempctrl.save"));
    QCOMPARE(QString::fromStdString(saveMaxVoltageReq.payload["params"].value("key", std::string())),
             QStringLiteral("max_voltage"));
}

void PanelSettingsTest::streamPanelRestoresSavedChannels()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/stream/raw16"), true);
    settings.setValue(QStringLiteral("panels/stream/sliceStitch16"), true);
    settings.setValue(QStringLiteral("panels/stream/spectralPreview"), true);

    DeviceClient device;
    StreamPanel panel(&device);

    auto* raw = panel.findChild<QCheckBox*>(QStringLiteral("streamRaw16Check"));
    auto* slice = panel.findChild<QCheckBox*>(QStringLiteral("streamSliceStitch16Check"));
    auto* spectralPreview = panel.findChild<QCheckBox*>(QStringLiteral("streamSpectralPreviewCheck"));

    QVERIFY(raw);
    QVERIFY(slice);
    QVERIFY(spectralPreview);
    QVERIFY(raw->isChecked());
    QVERIFY(slice->isChecked());
    QVERIFY(spectralPreview->isChecked());
    QCOMPARE(panel.selectedChannels(),
             QStringList({QStringLiteral("raw16"),
                          QStringLiteral("slice_stitch16"),
                          QStringLiteral("spectral_preview")}));
}

void PanelSettingsTest::spectralPanelRestoresSavedRenderInputs()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/spectral/sourceMode"), static_cast<int>(SpectralSourceMode::Live));
    settings.setValue(QStringLiteral("panels/spectral/sourceChannel"), 1);
    settings.setValue(QStringLiteral("panels/spectral/mode"), static_cast<int>(SpectralRenderOptions::RgbComposite));
    settings.setValue(QStringLiteral("panels/spectral/singleBand"), 5);
    settings.setValue(QStringLiteral("panels/spectral/rangeStart"), 6);
    settings.setValue(QStringLiteral("panels/spectral/rangeEnd"), 7);
    settings.setValue(QStringLiteral("panels/spectral/rBand"), 8);
    settings.setValue(QStringLiteral("panels/spectral/gBand"), 9);
    settings.setValue(QStringLiteral("panels/spectral/bBand"), 10);

    SpectralPanel panel;
    panel.setBandCount(16);

    auto* sourceMode = panel.findChild<QComboBox*>(QStringLiteral("spectralSourceModeCombo"));
    auto* source = panel.findChild<QComboBox*>(QStringLiteral("spectralSourceCombo"));
    auto* mode = panel.findChild<QComboBox*>(QStringLiteral("spectralModeCombo"));
    auto* rBand = panel.findChild<QSpinBox*>(QStringLiteral("spectralRBandSpin"));
    auto* gBand = panel.findChild<QSpinBox*>(QStringLiteral("spectralGBandSpin"));
    auto* bBand = panel.findChild<QSpinBox*>(QStringLiteral("spectralBBandSpin"));

    QVERIFY(sourceMode);
    QVERIFY(source);
    QVERIFY(mode);
    QVERIFY(rBand);
    QVERIFY(gBand);
    QVERIFY(bBand);
    QCOMPARE(sourceMode->currentData().toInt(), static_cast<int>(SpectralSourceMode::Live));
    QCOMPARE(source->currentData().toInt(), 1);
    QCOMPARE(mode->currentData().toInt(), static_cast<int>(SpectralRenderOptions::RgbComposite));
    QCOMPARE(rBand->value(), 8);
    QCOMPARE(gBand->value(), 9);
    QCOMPARE(bBand->value(), 10);
}

void PanelSettingsTest::spectralPanelShowsSavedBandsBeforeStreamMetadata()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/spectral/mode"), static_cast<int>(SpectralRenderOptions::RangeAverage));
    settings.setValue(QStringLiteral("panels/spectral/rangeStart"), 6);
    settings.setValue(QStringLiteral("panels/spectral/rangeEnd"), 7);

    SpectralPanel panel;

    auto* rangeStart = panel.findChild<QSpinBox*>(QStringLiteral("spectralRangeStartSpin"));
    auto* rangeEnd = panel.findChild<QSpinBox*>(QStringLiteral("spectralRangeEndSpin"));

    QVERIFY(rangeStart);
    QVERIFY(rangeEnd);
    QCOMPARE(rangeStart->value(), 6);
    QCOMPARE(rangeEnd->value(), 7);
}

void PanelSettingsTest::spectralPanelKeepsSavedBandsWhenStatsAreEmpty()
{
    QSettings settings;
    settings.setValue(QStringLiteral("panels/spectral/mode"), static_cast<int>(SpectralRenderOptions::RangeAverage));
    settings.setValue(QStringLiteral("panels/spectral/rangeStart"), 6);
    settings.setValue(QStringLiteral("panels/spectral/rangeEnd"), 7);

    SpectralPanel panel;
    panel.setBandCount(0);

    auto* rangeStart = panel.findChild<QSpinBox*>(QStringLiteral("spectralRangeStartSpin"));
    auto* rangeEnd = panel.findChild<QSpinBox*>(QStringLiteral("spectralRangeEndSpin"));

    QVERIFY(rangeStart);
    QVERIFY(rangeEnd);
    QCOMPARE(rangeStart->value(), 6);
    QCOMPARE(rangeEnd->value(), 7);
    QCOMPARE(QSettings().value(QStringLiteral("panels/spectral/rangeStart")).toInt(), 6);
    QCOMPARE(QSettings().value(QStringLiteral("panels/spectral/rangeEnd")).toInt(), 7);
}

void PanelSettingsTest::irPanelRestoresSavedUserInputs()
{
    QSettings settings;
    const QString p = QStringLiteral("panels/ir/");
    settings.setValue(p + QStringLiteral("brightness"), 21);
    settings.setValue(p + QStringLiteral("contrast"), 22);
    settings.setValue(p + QStringLiteral("dde"), 3);
    settings.setValue(p + QStringLiteral("abMode"), 1);
    settings.setValue(p + QStringLiteral("integration"), 1234);
    settings.setValue(p + QStringLiteral("integrationMode"), 1);
    settings.setValue(p + QStringLiteral("gearMode"), 1);
    settings.setValue(p + QStringLiteral("gearSelect"), 6);
    settings.setValue(p + QStringLiteral("imageType"), 2);
    settings.setValue(p + QStringLiteral("testPattern"), 1);
    settings.setValue(p + QStringLiteral("colorMode"), 1);
    settings.setValue(p + QStringLiteral("badPixelDisplay"), 1);
    settings.setValue(p + QStringLiteral("temporalFilterEnabled"), true);
    settings.setValue(p + QStringLiteral("temporalFilterCoeff"), 5);
    settings.setValue(p + QStringLiteral("medianFilterEnabled"), true);
    settings.setValue(p + QStringLiteral("medianFilterCoeff"), 33);
    settings.setValue(p + QStringLiteral("flipH"), 1);
    settings.setValue(p + QStringLiteral("flipV"), 1);
    settings.setValue(p + QStringLiteral("externalSync"), 1);
    settings.setValue(p + QStringLiteral("standby"), 1);
    settings.setValue(p + QStringLiteral("autoCalibration"), 1);
    settings.setValue(p + QStringLiteral("maintenanceUnlock"), 1);
    settings.setValue(p + QStringLiteral("maintenanceExecName"), QStringLiteral("clear_b"));
    settings.setValue(p + QStringLiteral("maintenanceExecValue"), 44);
    settings.setValue(p + QStringLiteral("clearK"), 1);
    settings.setValue(p + QStringLiteral("clearB"), 1);
    settings.setValue(p + QStringLiteral("badPixelSearch"), 7);
    settings.setValue(p + QStringLiteral("badPixelPos0"), 10);
    settings.setValue(p + QStringLiteral("badPixelPos1"), 20);
    settings.setValue(p + QStringLiteral("badPixelPos2"), 30);
    settings.setValue(p + QStringLiteral("badPixelPos3"), 40);

    DeviceClient device;
    IrPanel panel(&device);

    auto* brightness = panel.findChild<QSpinBox*>(QStringLiteral("irBrightnessSpin"));
    auto* contrast = panel.findChild<QSpinBox*>(QStringLiteral("irContrastSpin"));
    auto* dde = panel.findChild<QSpinBox*>(QStringLiteral("irDdeSpin"));
    auto* abMode = panel.findChild<QComboBox*>(QStringLiteral("irAbModeCombo"));
    auto* integration = panel.findChild<QSpinBox*>(QStringLiteral("irIntegrationSpin"));
    auto* integrationMode = panel.findChild<QComboBox*>(QStringLiteral("irIntegrationModeCombo"));
    auto* gearMode = panel.findChild<QComboBox*>(QStringLiteral("irGearModeCombo"));
    auto* gearSelect = panel.findChild<QComboBox*>(QStringLiteral("irGearSelectCombo"));
    auto* imageType = panel.findChild<QComboBox*>(QStringLiteral("irImageTypeCombo"));
    auto* testPattern = panel.findChild<QComboBox*>(QStringLiteral("irTestPatternCombo"));
    auto* colorMode = panel.findChild<QComboBox*>(QStringLiteral("irColorModeCombo"));
    auto* badPixelDisplay = panel.findChild<QComboBox*>(QStringLiteral("irBadPixelDisplayCombo"));
    auto* tempFilter = panel.findChild<QCheckBox*>(QStringLiteral("irTempFilterCheck"));
    auto* tempCoeff = panel.findChild<QSpinBox*>(QStringLiteral("irTempFilterCoeffSpin"));
    auto* medianFilter = panel.findChild<QCheckBox*>(QStringLiteral("irMedianFilterCheck"));
    auto* medianCoeff = panel.findChild<QSpinBox*>(QStringLiteral("irMedianFilterCoeffSpin"));
    auto* flipH = panel.findChild<QComboBox*>(QStringLiteral("irFlipHCombo"));
    auto* flipV = panel.findChild<QComboBox*>(QStringLiteral("irFlipVCombo"));
    auto* extSync = panel.findChild<QComboBox*>(QStringLiteral("irExtSyncCombo"));
    auto* standby = panel.findChild<QComboBox*>(QStringLiteral("irStandbyCombo"));
    auto* autoCalib = panel.findChild<QComboBox*>(QStringLiteral("irAutoCalibCombo"));
    auto* maintenanceUnlock = panel.findChild<QComboBox*>(QStringLiteral("irMaintenanceUnlockCombo"));
    auto* maintenanceExecName = panel.findChild<QComboBox*>(QStringLiteral("irMaintenanceExecNameCombo"));
    auto* maintenanceExecValue = panel.findChild<QSpinBox*>(QStringLiteral("irMaintenanceExecValueSpin"));
    auto* clearK = panel.findChild<QComboBox*>(QStringLiteral("irClearKCombo"));
    auto* clearB = panel.findChild<QComboBox*>(QStringLiteral("irClearBCombo"));
    auto* badPixelSearch = panel.findChild<QComboBox*>(QStringLiteral("irBadPixelSearchCombo"));
    auto* badPixelPos0 = panel.findChild<QSpinBox*>(QStringLiteral("irBadPixelPos0Spin"));
    auto* badPixelPos1 = panel.findChild<QSpinBox*>(QStringLiteral("irBadPixelPos1Spin"));
    auto* badPixelPos2 = panel.findChild<QSpinBox*>(QStringLiteral("irBadPixelPos2Spin"));
    auto* badPixelPos3 = panel.findChild<QSpinBox*>(QStringLiteral("irBadPixelPos3Spin"));

    QVERIFY(brightness);
    QVERIFY(contrast);
    QVERIFY(dde);
    QVERIFY(abMode);
    QVERIFY(integration);
    QVERIFY(integrationMode);
    QVERIFY(gearMode);
    QVERIFY(gearSelect);
    QVERIFY(imageType);
    QVERIFY(testPattern);
    QVERIFY(colorMode);
    QVERIFY(badPixelDisplay);
    QVERIFY(tempFilter);
    QVERIFY(tempCoeff);
    QVERIFY(medianFilter);
    QVERIFY(medianCoeff);
    QVERIFY(flipH);
    QVERIFY(flipV);
    QVERIFY(extSync);
    QVERIFY(standby);
    QVERIFY(autoCalib);
    QVERIFY(maintenanceUnlock);
    QVERIFY(maintenanceExecName);
    QVERIFY(maintenanceExecValue);
    QVERIFY(clearK);
    QVERIFY(clearB);
    QVERIFY(badPixelSearch);
    QVERIFY(badPixelPos0);
    QVERIFY(badPixelPos1);
    QVERIFY(badPixelPos2);
    QVERIFY(badPixelPos3);

    QCOMPARE(brightness->value(), 21);
    QCOMPARE(contrast->value(), 22);
    QCOMPARE(dde->value(), 3);
    QCOMPARE(abMode->currentData().toInt(), 1);
    QCOMPARE(integration->value(), 1234);
    QCOMPARE(integrationMode->currentData().toInt(), 1);
    QCOMPARE(gearMode->currentData().toInt(), 1);
    QCOMPARE(gearSelect->currentData().toInt(), 6);
    QCOMPARE(imageType->currentData().toInt(), 2);
    QCOMPARE(testPattern->currentData().toInt(), 1);
    QCOMPARE(colorMode->currentData().toInt(), 1);
    QCOMPARE(badPixelDisplay->currentData().toInt(), 1);
    QVERIFY(tempFilter->isChecked());
    QCOMPARE(tempCoeff->value(), 5);
    QVERIFY(medianFilter->isChecked());
    QCOMPARE(medianCoeff->value(), 33);
    QCOMPARE(flipH->currentData().toInt(), 1);
    QCOMPARE(flipV->currentData().toInt(), 1);
    QCOMPARE(extSync->currentData().toInt(), 1);
    QCOMPARE(standby->currentData().toInt(), 1);
    QCOMPARE(autoCalib->currentData().toInt(), 1);
    QCOMPARE(maintenanceUnlock->currentData().toInt(), 1);
    QCOMPARE(maintenanceExecName->currentData().toString(), QStringLiteral("clear_b"));
    QCOMPARE(maintenanceExecValue->value(), 44);
    QCOMPARE(clearK->currentData().toInt(), 1);
    QCOMPARE(clearB->currentData().toInt(), 1);
    QCOMPARE(badPixelSearch->currentData().toInt(), 7);
    QCOMPARE(badPixelPos0->value(), 10);
    QCOMPARE(badPixelPos1->value(), 20);
    QCOMPARE(badPixelPos2->value(), 30);
    QCOMPARE(badPixelPos3->value(), 40);
}

void PanelSettingsTest::irPanelSwitchesBetweenSeparateLegacyAndCi05Pages()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    IrPanel panel(&device);
    auto* stack = panel.findChild<QStackedWidget*>(QStringLiteral("irCoreModeStack"));
    auto* legacyPage = panel.findChild<QWidget*>(QStringLiteral("irLegacyPage"));
    auto* ci05Page = panel.findChild<QWidget*>(QStringLiteral("irCi05Page"));
    auto* currentModel = panel.findChild<QLabel*>(QStringLiteral("irCurrentModelLabel"));
    QVERIFY(stack);
    QVERIFY(legacyPage);
    QVERIFY(ci05Page);
    QVERIFY(currentModel);
    QCOMPARE(stack->currentWidget(), legacyPage);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    const CapturedControlRequest modelReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(modelReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.core.current"));
    writeControlResponse(socket, modelReq.header.seq, {{"model", "ci05"}});
    QTRY_COMPARE(currentModel->text(), QString::fromUtf8("当前机芯: ci05"));
    QCOMPARE(stack->currentWidget(), ci05Page);
}

void PanelSettingsTest::irPanelCi05PageUsesSeparateCi05Controls()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    IrPanel panel(&device);
    auto* ci05Brightness = panel.findChild<QSpinBox*>(QStringLiteral("irCi05BrightnessSpin"));
    auto* legacyBrightness = panel.findChild<QSpinBox*>(QStringLiteral("irBrightnessSpin"));
    auto* applyCi05Brightness = panel.findChild<QPushButton*>(QStringLiteral("irCi05ApplyBrightnessButton"));
    auto* applyCi05FrameRate = panel.findChild<QPushButton*>(QStringLiteral("irCi05ApplyFrameRateButton"));
    auto* currentModel = panel.findChild<QLabel*>(QStringLiteral("irCurrentModelLabel"));
    QVERIFY(ci05Brightness);
    QVERIFY(legacyBrightness);
    QVERIFY(applyCi05Brightness);
    QVERIFY(applyCi05FrameRate);
    QVERIFY(currentModel);
    QVERIFY(ci05Brightness != legacyBrightness);
    QVERIFY2(applyCi05FrameRate->text().contains(QStringLiteral("0.01 Hz")),
             qPrintable(applyCi05FrameRate->text()));

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    const CapturedControlRequest modelReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(modelReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.core.current"));
    writeControlResponse(socket, modelReq.header.seq, {{"model", "ci05"}});
    QTRY_COMPARE(currentModel->text(), QString::fromUtf8("当前机芯: ci05"));

    ci05Brightness->setValue(50);
    applyCi05Brightness->click();

    const CapturedControlRequest brightnessReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(brightnessReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.set_brightness"));
    QCOMPARE(brightnessReq.payload["params"].value("value", -1), 50);
}

void PanelSettingsTest::irPanelCi05PageReadsAllStatusRegisters()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    IrPanel panel(&device);
    auto* currentModel = panel.findChild<QLabel*>(QStringLiteral("irCurrentModelLabel"));
    auto* readStatus1 = panel.findChild<QPushButton*>(QStringLiteral("irCi05ReadStatus1Button"));
    auto* readStatus2 = panel.findChild<QPushButton*>(QStringLiteral("irCi05ReadStatus2Button"));
    auto* readStatus3 = panel.findChild<QPushButton*>(QStringLiteral("irCi05ReadStatus3Button"));
    auto* readStatus4 = panel.findChild<QPushButton*>(QStringLiteral("irCi05ReadStatus4Button"));
    QVERIFY(currentModel);
    QVERIFY(readStatus1);
    QVERIFY(readStatus2);
    QVERIFY(readStatus3);
    QVERIFY(readStatus4);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    const CapturedControlRequest modelReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(modelReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.core.current"));
    writeControlResponse(socket, modelReq.header.seq, {{"model", "ci05"}});
    QTRY_COMPARE(currentModel->text(), QString::fromUtf8("当前机芯: ci05"));

    readStatus2->click();
    CapturedControlRequest statusReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(statusReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.read_status2"));

    readStatus3->click();
    statusReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(statusReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.read_status3"));

    readStatus4->click();
    statusReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(statusReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.read_status4"));
}

void PanelSettingsTest::irPanelCi05PageTriggersAllCompensations()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    DeviceClient device;
    IrPanel panel(&device);
    auto* currentModel = panel.findChild<QLabel*>(QStringLiteral("irCurrentModelLabel"));
    auto* shutter = panel.findChild<QPushButton*>(QStringLiteral("irCi05TriggerShutterCompensationButton"));
    auto* scene = panel.findChild<QPushButton*>(QStringLiteral("irCi05TriggerSceneCompensationButton"));
    auto* defocus = panel.findChild<QPushButton*>(QStringLiteral("irCi05TriggerDefocusCompensationButton"));
    auto* integration = panel.findChild<QPushButton*>(QStringLiteral("irCi05TriggerIntegrationCorrectionButton"));
    QVERIFY(currentModel);
    QVERIFY(shutter);
    QVERIFY(scene);
    QVERIFY(defocus);
    QVERIFY(integration);

    device.connectTo(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(server.waitForNewConnection(1000));
    QTcpSocket* socket = server.nextPendingConnection();
    QVERIFY(socket);
    QTRY_VERIFY(device.isConnected());

    const CapturedControlRequest modelReq = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(modelReq.payload.value("cmd", std::string())),
             QStringLiteral("ir.core.current"));
    writeControlResponse(socket, modelReq.header.seq, {{"model", "ci05"}});
    QTRY_COMPARE(currentModel->text(), QString::fromUtf8("当前机芯: ci05"));

    shutter->click();
    CapturedControlRequest request = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.trigger_shutter_compensation"));

    scene->click();
    request = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.trigger_scene_compensation"));

    defocus->click();
    request = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.trigger_defocus_compensation"));

    integration->click();
    request = readControlRequest(socket);
    QCOMPARE(QString::fromStdString(request.payload.value("cmd", std::string())),
             QStringLiteral("ir.ci05.trigger_integration_correction"));
}

void PanelSettingsTest::collectPanelProvidesBackgroundCalibrationControls()
{
    DeviceClient device;
    CollectPanel panel(&device);
    auto* button = panel.findChild<QPushButton*>(QStringLiteral("collectBackgroundCalibrationButton"));
    auto* status = panel.findChild<QLabel*>(QStringLiteral("collectBackgroundCalibrationStatusLabel"));
    QVERIFY(button);
    QVERIFY(status);
    QCOMPARE(button->text(), QString::fromUtf8("开始背景矫正"));
    QCOMPARE(status->text(), QString::fromUtf8("未启动"));
}

QTEST_MAIN(PanelSettingsTest)

#include "PanelSettingsTest.moc"

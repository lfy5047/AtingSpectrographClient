#include <QtTest/QtTest>

#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>

#include "CameraPanel.h"
#include "CollectPanel.h"
#include "ConnectionPanel.h"
#include "DeviceClient.h"
#include "IrPanel.h"
#include "MirrorPanel.h"
#include "PanelSettings.h"
#include "SpectralPanel.h"
#include "StreamPanel.h"

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
    void streamPanelRestoresSavedChannels();
    void spectralPanelRestoresSavedRenderInputs();
    void irPanelRestoresSavedUserInputs();
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

QTEST_MAIN(PanelSettingsTest)

#include "PanelSettingsTest.moc"

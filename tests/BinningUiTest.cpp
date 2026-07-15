#include <QtTest/QtTest>

#include "Ui/BinningTestTypes.h"
#include "Ui/panels/BinningTestPanel.h"
#include "Ui/widgets/BinningCompareWidget.h"
#include "Ui/widgets/ImageView.h"
#include "Ui/widgets/ViewerAreaWidget.h"

#include "Protocol.h"
#include "Ui/ImageFrameUtils.h"

#include <QComboBox>
#include <QLabel>
#include <QSizePolicy>
#include <QTableWidget>
#include <cstring>

class BinningUiTest : public QObject {
    Q_OBJECT

private slots:
    void panelExposesOnlyRequiredFactors();
    void sourceSelectorDefaultsToRawAndOffersSlice();
    void resultTableOmitsVerdictColumn();
    void expectedSizeUsesFloorDivision();
    void featureWidthUsesOnePixelTolerance();
    void compareViewUsesCommonDisplayRange();
    void compareViewShowsHoverCoordinatesAndDn();
    void compareInfoLabelsAllowFullDisplay();
    void mainViewerShowsHoverDn();
    void viewerProvidesDedicatedBinningComparePage();
};

namespace {

QByteArray mono16Data(std::initializer_list<quint16> values)
{
    QByteArray bytes(static_cast<int>(values.size() * sizeof(quint16)), '\0');
    int index = 0;
    for (quint16 value : values) {
        std::memcpy(bytes.data() + index * static_cast<int>(sizeof(quint16)),
                    &value, sizeof(value));
        ++index;
    }
    return bytes;
}

BinningSnapshot snapshot(int factor, int width, int height,
                         std::initializer_list<quint16> values)
{
    BinningSnapshot result;
    result.factor = factor;
    result.width = width;
    result.height = height;
    result.data = mono16Data(values);
    result.stats = makeChannelImageStats(width, height, cli::proto::Mono16, result.data);
    return result;
}

} // namespace

void BinningUiTest::panelExposesOnlyRequiredFactors()
{
    BinningTestPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("binningFactorCombo"));

    QVERIFY(combo);
    QCOMPARE(combo->count(), 3);
    QCOMPARE(combo->itemData(0).toInt(), 1);
    QCOMPARE(combo->itemData(1).toInt(), 2);
    QCOMPARE(combo->itemData(2).toInt(), 4);
}

void BinningUiTest::sourceSelectorDefaultsToRawAndOffersSlice()
{
    BinningTestPanel panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("binningSourceCombo"));

    QVERIFY(combo);
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->currentData().toInt(), static_cast<int>(cli::proto::Raw16));
    QCOMPARE(combo->itemData(0).toInt(), static_cast<int>(cli::proto::Raw16));
    QCOMPARE(combo->itemData(1).toInt(), static_cast<int>(cli::proto::SliceStitch16));
}

void BinningUiTest::resultTableOmitsVerdictColumn()
{
    BinningTestPanel panel;
    auto* table = panel.findChild<QTableWidget*>(QStringLiteral("binningResultTable"));
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QString::fromUtf8("模式"));
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QString::fromUtf8("配置"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QString::fromUtf8("理论尺寸"));
    QCOMPARE(table->horizontalHeaderItem(3)->text(), QString::fromUtf8("实际尺寸"));
    QCOMPARE(table->horizontalHeaderItem(4)->text(), QString::fromUtf8("特征宽度"));

    panel.setCaptureResult(1, true, QSize(195, 512), QSize(195, 512));
    panel.setCaptureResult(2, true, QSize(97, 256), QSize(97, 256));
    panel.setCaptureResult(4, true, QSize(48, 128), QSize(48, 128));
    QCOMPARE(table->item(0, 4)->text(), QString::fromUtf8("未测量"));
    QCOMPARE(table->item(1, 4)->text(), QString::fromUtf8("未测量"));
    QCOMPARE(table->item(2, 4)->text(), QString::fromUtf8("未测量"));

    panel.setMeasurementResult(1, 64);
    panel.setMeasurementResult(2, 32);
    panel.setMeasurementResult(4, 20);
    QCOMPARE(table->item(0, 4)->text(), QStringLiteral("64 px"));
    QCOMPARE(table->item(1, 4)->text(), QStringLiteral("32 px"));
    QCOMPARE(table->item(2, 4)->text(), QStringLiteral("20 px"));
}

void BinningUiTest::expectedSizeUsesFloorDivision()
{
    const QSize baseline(1301, 1025);

    QCOMPARE(expectedBinningSize(baseline, 1), QSize(1301, 1025));
    QCOMPARE(expectedBinningSize(baseline, 2), QSize(650, 512));
    QCOMPARE(expectedBinningSize(baseline, 4), QSize(325, 256));
    QVERIFY(expectedBinningSize(baseline, 3).isEmpty());
}

void BinningUiTest::featureWidthUsesOnePixelTolerance()
{
    QVERIFY(binningFeatureWidthMatches(81, 40, 2, 1));
    QVERIFY(binningFeatureWidthMatches(81, 21, 4, 1));
    QVERIFY(!binningFeatureWidthMatches(81, 37, 2, 1));
    QVERIFY(!binningFeatureWidthMatches(81, 24, 4, 1));
}

void BinningUiTest::compareViewUsesCommonDisplayRange()
{
    BinningCompareWidget compare;
    compare.setSnapshot(snapshot(1, 2, 1, {0, 100}));
    compare.setSnapshot(snapshot(2, 1, 1, {50}));
    compare.setSnapshot(snapshot(4, 1, 1, {200}));

    ImageView* factor1 = compare.imageViewForFactor(1);
    ImageView* factor2 = compare.imageViewForFactor(2);
    ImageView* factor4 = compare.imageViewForFactor(4);
    QVERIFY(factor1);
    QVERIFY(factor2);
    QVERIFY(factor4);

    QCOMPARE(factor1->currentImage().pixelColor(0, 0).red(), 0);
    QVERIFY(qAbs(factor1->currentImage().pixelColor(1, 0).red() - 128) <= 1);
    QVERIFY(qAbs(factor2->currentImage().pixelColor(0, 0).red() - 64) <= 1);
    QCOMPARE(factor4->currentImage().pixelColor(0, 0).red(), 255);
}

void BinningUiTest::compareViewShowsHoverCoordinatesAndDn()
{
    BinningCompareWidget compare;
    compare.setSnapshot(snapshot(1, 2, 2, {10, 20, 30, 40}));

    ImageView* factor1 = compare.imageViewForFactor(1);
    auto* coordinate = compare.findChild<QLabel*>(QStringLiteral("binningCoordinateLabel1"));
    QVERIFY(factor1);
    QVERIFY(coordinate);
    QCOMPARE(coordinate->text(), QString::fromUtf8("坐标: --  DN: --"));

    QVERIFY(QMetaObject::invokeMethod(factor1, "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(1, 1))));
    QCOMPARE(coordinate->text(), QStringLiteral("X: 1  Y: 1  DN: 40"));

    QVERIFY(QMetaObject::invokeMethod(factor1, "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(-1, -1))));
    QCOMPARE(coordinate->text(), QString::fromUtf8("坐标: --  DN: --"));
}

void BinningUiTest::compareInfoLabelsAllowFullDisplay()
{
    BinningCompareWidget compare;
    compare.setSnapshot(snapshot(1, 2, 1, {0, 100}));

    auto* ratio = compare.findChild<QLabel*>(QStringLiteral("binningPixelRatioLabel1"));
    auto* stats = compare.findChild<QLabel*>(QStringLiteral("binningDnStatsLabel1"));
    auto* hint = compare.findChild<QLabel*>(QStringLiteral("binningMeasurementHintLabel"));
    QVERIFY(ratio);
    QVERIFY(stats);
    QVERIFY(hint);
    QVERIFY(stats->wordWrap());
    QVERIFY(hint->wordWrap());
    QCOMPARE(hint->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
    QVERIFY(ratio->text().contains(QString::fromUtf8("总像素比")));
    QVERIFY(stats->text().contains(QStringLiteral("Min")));
    QVERIFY(stats->text().contains(QStringLiteral("Avg")));
    QVERIFY(stats->text().contains(QStringLiteral("Max")));
}

void BinningUiTest::mainViewerShowsHoverDn()
{
    ViewerAreaWidget viewer;
    const QByteArray data = mono16Data({12, 345});
    viewer.setImageStats(ViewerAreaWidget::Raw16View,
                         makeChannelImageStats(2, 1, cli::proto::Mono16, data));
    viewer.renderFrame(ViewerAreaWidget::Raw16View, 2, 1, cli::proto::Mono16, data);

    ImageView* raw = viewer.imageView(ViewerAreaWidget::Raw16View);
    auto* info = viewer.findChild<QLabel*>(QStringLiteral("imgInfoLabel"));
    QVERIFY(raw);
    QVERIFY(info);

    QVERIFY(QMetaObject::invokeMethod(raw, "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(1, 0))));
    QCOMPARE(info->text(), QStringLiteral(
        "x: 1  y: 0  dn: 345\nmin: 12  max: 345  avg: 178.5"));

    QImage playback(1, 1, QImage::Format_Grayscale8);
    playback.fill(77);
    viewer.setChannelImage(ViewerAreaWidget::PlaybackView, playback);
    viewer.setCurrentChannel(ViewerAreaWidget::PlaybackView);
    ImageView* playbackView = viewer.imageView(ViewerAreaWidget::PlaybackView);
    QVERIFY(playbackView);
    QVERIFY(QMetaObject::invokeMethod(playbackView, "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(0, 0))));
    QCOMPARE(info->text().section(QLatin1Char('\n'), 0, 0),
             QStringLiteral("x: 0  y: 0  dn: 77"));
}

void BinningUiTest::viewerProvidesDedicatedBinningComparePage()
{
    ViewerAreaWidget viewer;

    QVERIFY(viewer.binningCompareWidget());
    viewer.setCurrentChannel(ViewerAreaWidget::BinningCompareView);
    QCOMPARE(viewer.currentChannel(), static_cast<int>(ViewerAreaWidget::BinningCompareView));
}

QTEST_MAIN(BinningUiTest)

#include "BinningUiTest.moc"

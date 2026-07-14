#include <QtTest/QtTest>

#include "Protocol.h"
#include "Ui/ImageFrameUtils.h"
#include "Ui/RoiTestTypes.h"
#include "Ui/panels/RoiTestPanel.h"
#include "Ui/widgets/ImageView.h"
#include "Ui/widgets/RoiCompareWidget.h"
#include "Ui/widgets/ViewerAreaWidget.h"

#include <QLabel>
#include <QSpinBox>
#include <cstring>

class RoiUiTest : public QObject {
    Q_OBJECT

private slots:
    void panelProvidesRequestedDefaults();
    void compareViewUsesCommonDisplayRange();
    void compareViewShowsHoverCoordinates();
    void viewerProvidesDedicatedRoiComparePage();
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

RoiSnapshot snapshot(int width, int height, std::initializer_list<quint16> values)
{
    RoiSnapshot result;
    result.width = width;
    result.height = height;
    result.data = mono16Data(values);
    result.stats = makeChannelImageStats(width, height, cli::proto::Mono16, result.data);
    return result;
}

} // namespace

void RoiUiTest::panelProvidesRequestedDefaults()
{
    RoiTestPanel panel;

    auto* sliceBegin = panel.findChild<QSpinBox*>(QStringLiteral("roiSliceBeginSpin"));
    auto* sliceEnd = panel.findChild<QSpinBox*>(QStringLiteral("roiSliceEndSpin"));
    auto* sliceHBegin = panel.findChild<QSpinBox*>(QStringLiteral("roiSliceHBeginSpin"));
    auto* sliceHEnd = panel.findChild<QSpinBox*>(QStringLiteral("roiSliceHEndSpin"));
    QVERIFY(sliceBegin);
    QVERIFY(sliceEnd);
    QVERIFY(sliceHBegin);
    QVERIFY(sliceHEnd);

    QCOMPARE(sliceBegin->value(), 240);
    QCOMPARE(sliceEnd->value(), 435);
    QCOMPARE(sliceHBegin->value(), 0);
    QCOMPARE(sliceHEnd->value(), 512);

    const RoiConfig config = panel.testConfig();
    QCOMPARE(config.sliceBegin, 240);
    QCOMPARE(config.sliceEnd, 435);
    QCOMPARE(config.sliceHBegin, 0);
    QCOMPARE(config.sliceHEnd, 512);
}

void RoiUiTest::compareViewUsesCommonDisplayRange()
{
    RoiCompareWidget compare;
    compare.setFullFrameSnapshot(snapshot(2, 1, {0, 100}));
    compare.setRoiSnapshot(snapshot(1, 1, {200}));

    ImageView* full = compare.fullFrameView();
    ImageView* roi = compare.roiView();
    QVERIFY(full);
    QVERIFY(roi);
    QCOMPARE(full->currentImage().pixelColor(0, 0).red(), 0);
    QVERIFY(qAbs(full->currentImage().pixelColor(1, 0).red() - 128) <= 1);
    QCOMPARE(roi->currentImage().pixelColor(0, 0).red(), 255);
}

void RoiUiTest::compareViewShowsHoverCoordinates()
{
    RoiCompareWidget compare;
    compare.setFullFrameSnapshot(snapshot(4, 2, {0, 1, 2, 3, 4, 5, 6, 7}));
    compare.setRoiSnapshot(snapshot(2, 2, {8, 9, 10, 11}));

    auto* fullCoordinate = compare.findChild<QLabel*>(
        QStringLiteral("roiFullFrameCoordinateLabel"));
    auto* roiCoordinate = compare.findChild<QLabel*>(
        QStringLiteral("roiTargetCoordinateLabel"));
    QVERIFY(fullCoordinate);
    QVERIFY(roiCoordinate);
    QCOMPARE(fullCoordinate->text(), QString::fromUtf8("坐标: --"));
    QCOMPARE(roiCoordinate->text(), QString::fromUtf8("坐标: --"));

    QVERIFY(QMetaObject::invokeMethod(compare.fullFrameView(), "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(3, 1))));
    QCOMPARE(fullCoordinate->text(), QStringLiteral("X: 3  Y: 1"));
    QCOMPARE(roiCoordinate->text(), QString::fromUtf8("坐标: --"));

    QVERIFY(QMetaObject::invokeMethod(compare.roiView(), "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(1, 0))));
    QCOMPARE(roiCoordinate->text(), QStringLiteral("X: 1  Y: 0"));

    QVERIFY(QMetaObject::invokeMethod(compare.fullFrameView(), "cursorImagePosChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QPoint, QPoint(-1, -1))));
    QCOMPARE(fullCoordinate->text(), QString::fromUtf8("坐标: --"));
    QCOMPARE(roiCoordinate->text(), QStringLiteral("X: 1  Y: 0"));
}

void RoiUiTest::viewerProvidesDedicatedRoiComparePage()
{
    ViewerAreaWidget viewer;

    QVERIFY(viewer.roiCompareWidget());
    viewer.setCurrentChannel(ViewerAreaWidget::RoiCompareView);
    QCOMPARE(viewer.currentChannel(), static_cast<int>(ViewerAreaWidget::RoiCompareView));
}

QTEST_MAIN(RoiUiTest)

#include "RoiUiTest.moc"

#include <QtTest/QtTest>

#include "Ui/panels/SpectralSegmentTestPanel.h"
#include "Ui/widgets/ImageView.h"
#include "Ui/widgets/ViewerAreaWidget.h"

#include <QLabel>
#include <QMouseEvent>
#include <QSignalSpy>

class SpectralSegmentTest : public QObject {
    Q_OBJECT

private slots:
    void panelDisplaysCoordinatesAndDistance()
    {
        SpectralSegmentTestPanel panel;

        panel.setLinePositions(12, 45);

        auto* first = panel.findChild<QLabel*>(QStringLiteral("spectralSegmentFirstX"));
        auto* second = panel.findChild<QLabel*>(QStringLiteral("spectralSegmentSecondX"));
        auto* distance = panel.findChild<QLabel*>(QStringLiteral("spectralSegmentDistance"));
        QVERIFY(first);
        QVERIFY(second);
        QVERIFY(distance);
        QCOMPARE(first->text(), QStringLiteral("12 px"));
        QCOMPARE(second->text(), QStringLiteral("45 px"));
        QCOMPARE(distance->text(), QStringLiteral("33 px"));

        panel.setLinePositions(-1, -1);
        QCOMPARE(first->text(), QStringLiteral("--"));
        QCOMPARE(second->text(), QStringLiteral("--"));
        QCOMPARE(distance->text(), QStringLiteral("--"));
    }

    void viewerInitializesLinesFromRawImageWidth()
    {
        ViewerAreaWidget viewer;
        QSignalSpy spy(&viewer, &ViewerAreaWidget::spectralSegmentPositionsChanged);
        QImage image(300, 120, QImage::Format_Grayscale8);
        image.fill(Qt::black);

        viewer.setChannelImage(ViewerAreaWidget::Raw16View, image);
        viewer.setSpectralSegmentTestEnabled(true);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> arguments = spy.takeFirst();
        QCOMPARE(arguments.at(0).toInt(), 99);
        QCOMPARE(arguments.at(1).toInt(), 199);
    }

    void imageViewDragsVerticalLineInImageCoordinates()
    {
        ImageView view;
        view.resize(600, 300);
        QImage image(300, 150, QImage::Format_Grayscale8);
        image.fill(Qt::black);
        view.setImage(image);
        view.setSpectralSegmentTestEnabled(true);
        view.setSpectralSegmentLines(100, 200);
        QSignalSpy spy(&view, &ImageView::spectralSegmentLineMoveRequested);

        QMouseEvent press(QEvent::MouseButtonPress, QPointF(201, 150),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &press);
        QMouseEvent move(QEvent::MouseMove, QPointF(301, 150),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(301, 150),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &release);

        QVERIFY(!spy.isEmpty());
        const QList<QVariant> arguments = spy.takeLast();
        QCOMPARE(arguments.at(0).toInt(), 0);
        QCOMPARE(arguments.at(1).toInt(), 150);
    }

    void imageViewShowsHorizontalResizeCursorNearLine()
    {
        ImageView view;
        view.resize(600, 300);
        QImage image(300, 150, QImage::Format_Grayscale8);
        image.fill(Qt::black);
        view.setImage(image);
        view.setSpectralSegmentTestEnabled(true);
        view.setSpectralSegmentLines(100, 200);

        QMouseEvent overLine(QEvent::MouseMove, QPointF(201, 150),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &overLine);
        QCOMPARE(view.cursor().shape(), Qt::SizeHorCursor);

        QMouseEvent awayFromLine(QEvent::MouseMove, QPointF(101, 150),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &awayFromLine);
        QCOMPARE(view.cursor().shape(), Qt::ArrowCursor);

        view.setSpectralSegmentTestEnabled(false);
        QCOMPARE(view.cursor().shape(), Qt::ArrowCursor);
    }
};

QTEST_MAIN(SpectralSegmentTest)

#include "SpectralSegmentTest.moc"

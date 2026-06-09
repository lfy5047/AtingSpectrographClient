#include <QtTest/QtTest>

#include <QBuffer>
#include <QImage>

#include "ImageFrameUtils.h"

class ImageFrameUtilsTest : public QObject {
    Q_OBJECT

private slots:
    void decodesJpegPayload();
};

void ImageFrameUtilsTest::decodesJpegPayload()
{
    QImage source(8, 8, QImage::Format_RGB32);
    source.fill(Qt::red);

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(source.save(&buffer, "JPG", 95));

    const QImage decoded = makeDisplayImage(8, 8, 3, jpeg);
    QVERIFY(!decoded.isNull());
    QCOMPARE(decoded.width(), 8);
    QCOMPARE(decoded.height(), 8);

    const QColor pixel = decoded.pixelColor(4, 4);
    QVERIFY(pixel.red() > 180);
    QVERIFY(pixel.green() < 80);
    QVERIFY(pixel.blue() < 80);
}

QTEST_MAIN(ImageFrameUtilsTest)

#include "ImageFrameUtilsTest.moc"

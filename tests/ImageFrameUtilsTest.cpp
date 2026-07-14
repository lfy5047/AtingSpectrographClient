#include <QtTest/QtTest>

#include <QBuffer>
#include <QImage>

#include "ImageFrameUtils.h"

class ImageFrameUtilsTest : public QObject {
    Q_OBJECT

private slots:
    void decodesJpegPayload();
    void rendersMono16WithFixedDisplayRange();
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

void ImageFrameUtilsTest::rendersMono16WithFixedDisplayRange()
{
    const quint16 pixels[] = {100, 150, 200};
    const QByteArray data(reinterpret_cast<const char*>(pixels), sizeof(pixels));

    const QImage image = makeMono16DisplayImage(3, 1, data, 100, 200);

    QVERIFY(!image.isNull());
    QCOMPARE(image.pixelColor(0, 0).red(), 0);
    QVERIFY(qAbs(image.pixelColor(1, 0).red() - 128) <= 1);
    QCOMPARE(image.pixelColor(2, 0).red(), 255);
}

QTEST_MAIN(ImageFrameUtilsTest)

#include "ImageFrameUtilsTest.moc"

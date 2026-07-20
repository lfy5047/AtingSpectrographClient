#include <QtTest/QtTest>

#include <QBuffer>
#include <QImage>

#include "ImageFrameUtils.h"

class ImageFrameUtilsTest : public QObject {
    Q_OBJECT

private slots:
    void decodesJpegPayload();
    void rendersMono16WithFixedDisplayRange();
    void stretchesAndCropsMono16Horizontally();
    void stretchCropKeepsDataWhenDisabled();
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

void ImageFrameUtilsTest::stretchesAndCropsMono16Horizontally()
{
    const quint16 pixels[] = {
        0, 100, 200, 300,
        1000, 1100, 1200, 1300,
    };
    const QByteArray data(reinterpret_cast<const char*>(pixels), sizeof(pixels));

    const QByteArray result = stretchCropMono16Horizontal(data, 4, 2, 7, 1);

    QCOMPARE(result.size(), static_cast<int>(sizeof(pixels)));
    const auto* transformed = reinterpret_cast<const quint16*>(result.constData());
    const quint16 expected[] = {
        50, 100, 150, 200,
        1050, 1100, 1150, 1200,
    };
    for (int i = 0; i < 8; ++i) {
        QCOMPARE(transformed[i], expected[i]);
    }
}

void ImageFrameUtilsTest::stretchCropKeepsDataWhenDisabled()
{
    const quint16 pixels[] = {10, 20, 30, 40};
    const QByteArray data(reinterpret_cast<const char*>(pixels), sizeof(pixels));

    QCOMPARE(stretchCropMono16Horizontal(data, 4, 1, 0, 0), data);
    QCOMPARE(stretchCropMono16Horizontal(data, 4, 1, 4, 2), data);

    const QByteArray clamped = stretchCropMono16Horizontal(data, 4, 1, 6, 99);
    const auto* transformed = reinterpret_cast<const quint16*>(clamped.constData());
    const quint16 expected[] = {22, 28, 34, 40};
    for (int i = 0; i < 4; ++i) {
        QCOMPARE(transformed[i], expected[i]);
    }
}

QTEST_MAIN(ImageFrameUtilsTest)

#include "ImageFrameUtilsTest.moc"

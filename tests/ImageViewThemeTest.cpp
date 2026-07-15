#include <QtTest/QtTest>

#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QFrame>

#include <algorithm>

#include "ThemeManager.h"
#include "widgets/TopBarWidget.h"
#include "widgets/ImageView.h"
#include "widgets/ViewerAreaWidget.h"

class ImageViewThemeTest : public QObject {
    Q_OBJECT

private slots:
    void outdoorThemeUsesLightNoSignalBackground();
    void outdoorThemePaintsTopBarBackground();
    void outdoorThemePaintsViewerSpacingBackground();
    void viewerExposesNucRaw16Channel();
    void topBarExposesConnectionToggleButton();
    void topBarCombinesFpsAndDropped();
};

void ImageViewThemeTest::outdoorThemeUsesLightNoSignalBackground()
{
    ThemeManager::setCurrentThemeForTesting(ThemeManager::Theme::OutdoorLight);

    ImageView view;
    view.resize(96, 72);
    view.setNoSignal();

    QImage rendered(view.size(), QImage::Format_ARGB32);
    rendered.fill(Qt::transparent);
    QPainter painter(&rendered);
    view.render(&painter);

    const QColor pixel = rendered.pixelColor(4, 4);
    QVERIFY2(pixel.red() > 220 && pixel.green() > 225 && pixel.blue() > 230,
             qPrintable(QStringLiteral("expected light background, got #%1%2%3")
                 .arg(pixel.red(), 2, 16, QLatin1Char('0'))
                 .arg(pixel.green(), 2, 16, QLatin1Char('0'))
                 .arg(pixel.blue(), 2, 16, QLatin1Char('0'))));

    ThemeManager::setCurrentThemeForTesting(ThemeManager::Theme::IndustrialDark);
}

void ImageViewThemeTest::outdoorThemePaintsTopBarBackground()
{
    QVERIFY(ThemeManager::applyTheme(*qApp, ThemeManager::Theme::OutdoorLight));

    TopBarWidget topBar;
    topBar.resize(420, 110);

    QImage rendered(topBar.size(), QImage::Format_ARGB32);
    rendered.fill(Qt::black);
    QPainter painter(&rendered);
    topBar.render(&painter);

    const QColor pixel = rendered.pixelColor(4, 4);
    QVERIFY2(pixel.red() > 220 && pixel.green() > 225 && pixel.blue() > 230,
             qPrintable(QStringLiteral("expected light topbar background, got #%1%2%3")
                 .arg(pixel.red(), 2, 16, QLatin1Char('0'))
                 .arg(pixel.green(), 2, 16, QLatin1Char('0'))
                 .arg(pixel.blue(), 2, 16, QLatin1Char('0'))));

    ThemeManager::setCurrentThemeForTesting(ThemeManager::Theme::IndustrialDark);
}

void ImageViewThemeTest::outdoorThemePaintsViewerSpacingBackground()
{
    QVERIFY(ThemeManager::applyTheme(*qApp, ThemeManager::Theme::OutdoorLight));

    ViewerAreaWidget viewer;
    viewer.resize(420, 300);

    QImage rendered(viewer.size(), QImage::Format_ARGB32);
    rendered.fill(Qt::black);
    QPainter painter(&rendered);
    viewer.render(&painter);

    const QColor pixel = rendered.pixelColor(8, 40);
    QVERIFY2(pixel.red() > 220 && pixel.green() > 225 && pixel.blue() > 230,
             qPrintable(QStringLiteral("expected light viewer spacing background, got #%1%2%3")
                 .arg(pixel.red(), 2, 16, QLatin1Char('0'))
                 .arg(pixel.green(), 2, 16, QLatin1Char('0'))
                 .arg(pixel.blue(), 2, 16, QLatin1Char('0'))));

    ThemeManager::setCurrentThemeForTesting(ThemeManager::Theme::IndustrialDark);
}

void ImageViewThemeTest::viewerExposesNucRaw16Channel()
{
    ViewerAreaWidget viewer;
    QVERIFY(viewer.imageView(ViewerAreaWidget::NucRaw16View));

    QSignalSpy spy(&viewer, &ViewerAreaWidget::channelChanged);
    viewer.setCurrentChannel(ViewerAreaWidget::NucRaw16View);

    QCOMPARE(viewer.currentChannel(), static_cast<int>(ViewerAreaWidget::NucRaw16View));
    QCOMPARE(spy.count(), 1);
}

void ImageViewThemeTest::topBarExposesConnectionToggleButton()
{
    TopBarWidget topBar;
    auto* button = topBar.findChild<QPushButton*>(QStringLiteral("topBarConnectionButton"));
    QVERIFY(button);
    auto* parentCard = qobject_cast<QFrame*>(button->parentWidget());
    QVERIFY(parentCard);
    QVERIFY(parentCard->property("connectionCard").toBool());
    QVERIFY(button->property("compactAction").toBool());
    QVERIFY(button->maximumWidth() <= 72);
    QCOMPARE(button->text(), QStringLiteral("\u8fde\u63a5"));

    QSignalSpy spy(&topBar, SIGNAL(connectToggleRequested()));
    button->click();
    QCOMPARE(spy.count(), 1);

    topBar.setConnected(true, QStringLiteral("10.0.0.42"));
    QCOMPARE(button->text(), QStringLiteral("\u65ad\u5f00"));

    topBar.setConnected(false);
    QCOMPARE(button->text(), QStringLiteral("\u8fde\u63a5"));
}

void ImageViewThemeTest::topBarCombinesFpsAndDropped()
{
    TopBarWidget topBar;
    topBar.setFps(12.3);
    topBar.setDropped(4);

    const auto labels = topBar.findChildren<QLabel*>();
    const bool hasCombinedMetric = std::any_of(labels.begin(), labels.end(), [](const QLabel* label) {
        return label->text() == QStringLiteral("12.3 / 4");
    });
    QVERIFY(hasCombinedMetric);
}

QTEST_MAIN(ImageViewThemeTest)

#include "ImageViewThemeTest.moc"

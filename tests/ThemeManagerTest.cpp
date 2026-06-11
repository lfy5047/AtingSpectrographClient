#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSettings>

#include "ThemeManager.h"

class ThemeManagerTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void exposesThemeMetadata();
    void packagesThemeResources();
    void outdoorThemeProvidesDialogAndContainerCoverage();
    void fallsBackToIndustrialForUnknownIds();
    void tracksCurrentTheme();
    void savesAndRestoresThemeChoice();
};

void ThemeManagerTest::initTestCase()
{
    const QString settingsRoot = QDir::tempPath() + QStringLiteral("/AtingSpectrographClientThemeManagerTest");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot);
    QCoreApplication::setOrganizationName(QStringLiteral("AtingSpectrographTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ThemeManagerTest"));
    QSettings().clear();
}

void ThemeManagerTest::exposesThemeMetadata()
{
    const QVector<ThemeManager::ThemeInfo> themes = ThemeManager::availableThemes();

    QCOMPARE(themes.size(), 2);
    QCOMPARE(themes[0].theme, ThemeManager::Theme::IndustrialDark);
    QCOMPARE(themes[0].id, QStringLiteral("industrial"));
    QCOMPARE(themes[0].displayName, QString::fromUtf8("深色"));
    QCOMPARE(themes[0].resourcePath, QStringLiteral(":/style/industrial.qss"));

    QCOMPARE(themes[1].theme, ThemeManager::Theme::OutdoorLight);
    QCOMPARE(themes[1].id, QStringLiteral("outdoor_light"));
    QCOMPARE(themes[1].displayName, QString::fromUtf8("户外亮色"));
    QCOMPARE(themes[1].resourcePath, QStringLiteral(":/style/outdoor_light.qss"));
}

void ThemeManagerTest::packagesThemeResources()
{
    const QVector<ThemeManager::ThemeInfo> themes = ThemeManager::availableThemes();
    for (const ThemeManager::ThemeInfo& info : themes) {
        QVERIFY2(QFile::exists(info.resourcePath),
                 qPrintable(QStringLiteral("missing theme resource: %1").arg(info.resourcePath)));
    }
}

void ThemeManagerTest::outdoorThemeProvidesDialogAndContainerCoverage()
{
    QFile qss(ThemeManager::resourcePath(ThemeManager::Theme::OutdoorLight));
    QVERIFY(qss.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(qss.readAll());

    QVERIFY2(content.contains(QStringLiteral("QDialog")),
             "outdoor theme must style top-level dialogs");
    QVERIFY2(content.contains(QStringLiteral("QWidget#mainCentral")),
             "outdoor theme must style the central widget");
    QVERIFY2(content.contains(QStringLiteral("QWidget#mainContent")),
             "outdoor theme must style the main content widget");
    QVERIFY2(content.contains(QStringLiteral("QWidget#sidebarNav")),
             "outdoor theme must style the sidebar stretch area");
    QVERIFY2(content.contains(QStringLiteral("QWidget#viewerContainer")),
             "outdoor theme must style the viewer container");
    QVERIFY2(content.contains(QStringLiteral("QStackedWidget#viewerStack")),
             "outdoor theme must style the viewer stack");
    QVERIFY2(content.contains(QStringLiteral("QWidget#imageStatsOverlay"))
             && content.contains(QStringLiteral("rgba(255, 255, 255")),
             "outdoor theme must use a light image stats overlay");
    QVERIFY2(content.contains(QStringLiteral("QDialog#recordsDialog")),
             "outdoor theme must style the remote-record dialog");
    QVERIFY2(content.contains(QStringLiteral("QCustomPlot#spectrumCurvePlot")),
             "outdoor theme must style the spectrum curve plot frame");
}

void ThemeManagerTest::fallsBackToIndustrialForUnknownIds()
{
    QCOMPARE(ThemeManager::themeFromId(QStringLiteral("outdoor_light")),
             ThemeManager::Theme::OutdoorLight);
    QCOMPARE(ThemeManager::themeFromId(QStringLiteral("missing")),
             ThemeManager::Theme::IndustrialDark);
    QCOMPARE(ThemeManager::resourcePath(ThemeManager::Theme::IndustrialDark),
             QStringLiteral(":/style/industrial.qss"));
}

void ThemeManagerTest::tracksCurrentTheme()
{
    QCOMPARE(ThemeManager::currentTheme(), ThemeManager::Theme::IndustrialDark);

    ThemeManager::setCurrentThemeForTesting(ThemeManager::Theme::OutdoorLight);
    QCOMPARE(ThemeManager::currentTheme(), ThemeManager::Theme::OutdoorLight);

    ThemeManager::setCurrentThemeForTesting(ThemeManager::Theme::IndustrialDark);
    QCOMPARE(ThemeManager::currentTheme(), ThemeManager::Theme::IndustrialDark);
}

void ThemeManagerTest::savesAndRestoresThemeChoice()
{
    QSettings().clear();

    QCOMPARE(ThemeManager::loadSavedTheme(), ThemeManager::Theme::IndustrialDark);

    ThemeManager::saveTheme(ThemeManager::Theme::OutdoorLight);
    QCOMPARE(ThemeManager::loadSavedTheme(), ThemeManager::Theme::OutdoorLight);

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("ui/theme")).toString(),
             QStringLiteral("outdoor_light"));
}

QTEST_APPLESS_MAIN(ThemeManagerTest)

#include "ThemeManagerTest.moc"

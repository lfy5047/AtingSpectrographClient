#include <QtTest/QtTest>
#include <QRegularExpression>

#include "AppVersion.h"

class AppVersionTest : public QObject {
    Q_OBJECT

private slots:
    void exposesSemanticVersion();
    void buildsDisplayStrings();
};

void AppVersionTest::exposesSemanticVersion()
{
    const QString version = QString::fromLatin1(AppVersion::version());

    QVERIFY2(QRegularExpression(QStringLiteral("^\\d+\\.\\d+\\.\\d+$")).match(version).hasMatch(),
             qPrintable(QStringLiteral("invalid semantic version: %1").arg(version)));
    QCOMPARE(version,
             QStringLiteral("%1.%2.%3")
                 .arg(AppVersion::versionMajor())
                 .arg(AppVersion::versionMinor())
                 .arg(AppVersion::versionPatch()));
}

void AppVersionTest::buildsDisplayStrings()
{
    QCOMPARE(QString::fromLatin1(AppVersion::applicationName()),
             QStringLiteral("AtingSpectrographClient"));
    QCOMPARE(AppVersion::windowTitle(),
             QString::fromUtf8("AtingSpectrograph Client - Spectra Pro v%1")
                 .arg(QString::fromLatin1(AppVersion::version())));
}

QTEST_MAIN(AppVersionTest)

#include "AppVersionTest.moc"

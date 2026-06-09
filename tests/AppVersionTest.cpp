#include <QtTest/QtTest>
#include <QProcess>
#include <QRegularExpression>

#include "AppVersion.h"

namespace {

QString expectedBaseVersionFromGit()
{
    QProcess git;
    git.start(QStringLiteral("git"),
              {QStringLiteral("describe"),
               QStringLiteral("--tags"),
               QStringLiteral("--dirty"),
               QStringLiteral("--always"),
               QStringLiteral("--long")});
    if (!git.waitForFinished(5000) || git.exitCode() != 0) {
        return QString();
    }

    QString describe = QString::fromLocal8Bit(git.readAllStandardOutput()).trimmed();
    describe.remove(QRegularExpression(QStringLiteral("^[vV]")));

    QRegularExpression taggedPattern(QStringLiteral("^(\\d+)\\.(\\d+)\\.(\\d+)-(\\d+)-(g[0-9a-f]+)(-dirty)?$"));
    QRegularExpressionMatch taggedMatch = taggedPattern.match(describe);
    if (taggedMatch.hasMatch()) {
        const QString base = QStringLiteral("%1.%2.%3")
            .arg(taggedMatch.captured(1))
            .arg(taggedMatch.captured(2))
            .arg(taggedMatch.captured(3));
        return base;
    }

    QRegularExpression untaggedPattern(QStringLiteral("^([0-9a-f]+)(-dirty)?$"));
    QRegularExpressionMatch untaggedMatch = untaggedPattern.match(describe);
    if (untaggedMatch.hasMatch()) {
        return QStringLiteral("0.0.0");
    }

    return QString();
}

} // namespace

class AppVersionTest : public QObject {
    Q_OBJECT

private slots:
    void exposesSemanticVersion();
    void buildsDisplayStrings();
};

void AppVersionTest::exposesSemanticVersion()
{
    const QString version = QString::fromLatin1(AppVersion::version());
    const QString baseVersion = QString::fromLatin1(AppVersion::baseVersion());

    QVERIFY2(QRegularExpression(QStringLiteral("^\\d+\\.\\d+\\.\\d+$")).match(version).hasMatch(),
             qPrintable(QStringLiteral("invalid semantic version: %1").arg(version)));
    QCOMPARE(version, expectedBaseVersionFromGit());
    QCOMPARE(baseVersion, version);
    QCOMPARE(baseVersion,
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

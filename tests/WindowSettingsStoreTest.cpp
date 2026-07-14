#include <QtTest/QtTest>

#include "Ui/MainWindowPanelRegistry.h"
#include "Ui/WindowSettingsStore.h"

#include <QSettings>

class WindowSettingsStoreTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void migratesVersion6LogIndexPastBinningPanel();
    void migratesVersion7LogIndexPastRoiPanel();
    void keepsVersion7BinningIndex();
    void keepsVersion8RoiIndex();
};

void WindowSettingsStoreTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("AtingSpectrographTests"));
    QCoreApplication::setApplicationName(QStringLiteral("WindowSettingsStoreTest"));
}

void WindowSettingsStoreTest::init()
{
    QSettings settings;
    settings.clear();
    settings.sync();
}

void WindowSettingsStoreTest::migratesVersion6LogIndexPastBinningPanel()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/panelVersion"), 6);
    settings.setValue(QStringLiteral("window/panel"), 9);
    settings.sync();

    QCOMPARE(WindowSettingsStore::restore(nullptr, nullptr, nullptr),
             static_cast<int>(MainWindowPanelRegistry::Log));
}

void WindowSettingsStoreTest::keepsVersion7BinningIndex()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/panelVersion"), 7);
    settings.setValue(QStringLiteral("window/panel"), 9);
    settings.sync();

    QCOMPARE(WindowSettingsStore::restore(nullptr, nullptr, nullptr),
             static_cast<int>(MainWindowPanelRegistry::BinningTest));
}

void WindowSettingsStoreTest::migratesVersion7LogIndexPastRoiPanel()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/panelVersion"), 7);
    settings.setValue(QStringLiteral("window/panel"), 10);
    settings.sync();

    QCOMPARE(WindowSettingsStore::restore(nullptr, nullptr, nullptr),
             static_cast<int>(MainWindowPanelRegistry::Log));
}

void WindowSettingsStoreTest::keepsVersion8RoiIndex()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/panelVersion"), 8);
    settings.setValue(QStringLiteral("window/panel"), 10);
    settings.sync();

    QCOMPARE(WindowSettingsStore::restore(nullptr, nullptr, nullptr),
             static_cast<int>(MainWindowPanelRegistry::RoiTest));
}

QTEST_MAIN(WindowSettingsStoreTest)

#include "WindowSettingsStoreTest.moc"

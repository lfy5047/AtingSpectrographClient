#include "Client/recording/LocalRecordScanner.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

class LocalRecordScannerTest : public QObject {
    Q_OBJECT

private slots:
    void scansManifestRecords();
    void infersRawRecordsWithoutManifest();
    void skipsIncompleteRecords();
};

namespace {

bool writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(data) == data.size();
}

} // namespace

void LocalRecordScannerTest::scansManifestRecords()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString recordDir = QDir(temp.path()).filePath("raw/1001");
    const QByteArray rawData(8, '\1');
    const QByteArray jsonData = R"({"width":2,"height":2,"frame_count":1,"frames":[{}]})";
    QVERIFY(writeFile(QDir(recordDir).filePath("1001.raw"), rawData));
    QVERIFY(writeFile(QDir(recordDir).filePath("1001.json"), jsonData));
    const QByteArray manifest = QByteArray(
        QString(R"({"manifest_version":1,"type":"raw","record_id":"1001","timestamp_ns":123456789,"files":[{"name":"1001.raw","size_bytes":%1},{"name":"1001.json","size_bytes":%2}]})")
            .arg(rawData.size())
            .arg(jsonData.size())
            .toUtf8());
    QVERIFY(writeFile(QDir(recordDir).filePath("manifest.json"), manifest));

    QString err;
    const QVector<LocalRecordScanner::Record> records =
        LocalRecordScanner::scan(temp.path(), "raw", &err);

    QCOMPARE(err, QString());
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].recordId, QString("1001"));
    QCOMPARE(records[0].type, QString("raw"));
    QCOMPARE(records[0].timestampNs, quint64(123456789));
    QCOMPARE(records[0].rootPath, QDir(temp.path()).absolutePath());
    QCOMPARE(records[0].files.size(), 2);
}

void LocalRecordScannerTest::infersRawRecordsWithoutManifest()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString recordDir = QDir(temp.path()).filePath("raw/2002");
    QVERIFY(writeFile(QDir(recordDir).filePath("frame.raw"), QByteArray(16, '\2')));
    QVERIFY(writeFile(QDir(recordDir).filePath("frame.json"),
                      R"({"width":2,"height":2,"frame_count":2,"frames":[{},{}]})"));

    QString err;
    const QVector<LocalRecordScanner::Record> records =
        LocalRecordScanner::scan(temp.path(), "raw", &err);

    QCOMPARE(err, QString());
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].recordId, QString("2002"));
    QCOMPARE(records[0].files.size(), 2);
    QVERIFY(records[0].files[0].sizeBytes > 0);
}

void LocalRecordScannerTest::skipsIncompleteRecords()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(writeFile(QDir(temp.path()).filePath("raw/3003/only.json"),
                      R"({"width":2,"height":2,"frame_count":1,"frames":[{}]})"));
    QVERIFY(writeFile(QDir(temp.path()).filePath("tif/not-a-number/image.tif"), QByteArray(16, '\3')));

    QString err;
    const QVector<LocalRecordScanner::Record> rawRecords =
        LocalRecordScanner::scan(temp.path(), "raw", &err);
    QCOMPARE(err, QString());
    QVERIFY(rawRecords.isEmpty());

    const QVector<LocalRecordScanner::Record> tifRecords =
        LocalRecordScanner::scan(temp.path(), "tif", &err);
    QCOMPARE(err, QString());
    QVERIFY(tifRecords.isEmpty());
}

QTEST_MAIN(LocalRecordScannerTest)

#include "LocalRecordScannerTest.moc"

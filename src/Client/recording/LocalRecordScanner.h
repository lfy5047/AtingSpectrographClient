#pragma once

#include <QString>
#include <QVector>

class LocalRecordScanner {
public:
    struct File {
        QString name;
        quint64 sizeBytes = 0;
    };

    struct Record {
        QString rootPath;
        QString type;
        QString recordId;
        quint64 recordIdValue = 0;
        quint64 timestampNs = 0;
        QVector<File> files;
    };

    static QVector<Record> scan(const QString& rootPath, const QString& type, QString* err = nullptr);
};

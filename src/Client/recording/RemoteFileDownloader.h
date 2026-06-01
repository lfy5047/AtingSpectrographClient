#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

class QFile;
class QTcpSocket;

struct RemoteFetchFile {
    QString recordId;
    QString name;
    quint64 sizeBytes = 0;
};
Q_DECLARE_METATYPE(RemoteFetchFile)
Q_DECLARE_METATYPE(QVector<RemoteFetchFile>)

class RemoteFileDownloader : public QObject {
    Q_OBJECT
public:
    explicit RemoteFileDownloader(QObject* parent = nullptr);
    ~RemoteFileDownloader() override;

    bool isRunning() const { return running_; }

public slots:
    void start(const QString& host, quint16 port, const QString& transferId,
               const QString& type, const QVector<RemoteFetchFile>& files,
               const QString& cacheRoot);
    void cancel();

signals:
    void progress(quint64 receivedBytes, quint64 totalBytes, QString currentFile);
    void finished(QStringList recordIds);
    void failed(QString error);
    void canceled();

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError();

private:
    struct ExpectedFile {
        QString recordId;
        QString name;
        quint64 sizeBytes = 0;
        QString tempPath;
        QString finalPath;
    };

    struct ActiveFile {
        bool seen = false;
        QString name;
        quint64 fileSize = 0;
        quint64 received = 0;
        QFile* file = nullptr;
    };

    bool prepareExpectedFiles(const QVector<RemoteFetchFile>& files, QString* err);
    bool prepareDirectories(QString* err);
    void cleanupSocket();
    void cleanupOpenFile();
    void cleanupTempFiles();
    void fail(const QString& err);
    void complete();
    bool consumeAvailable(QString* err);
    bool consumeOneChunk(QString* err, bool* consumed);
    bool isSafeFileName(const QString& name) const;
    bool allExpectedFilesComplete() const;
    bool openActiveFile(quint32 fileIndex, const QString& name, quint64 fileSize, QString* err);
    bool writePayload(quint32 fileIndex, quint64 offset, const QByteArray& payload, QString* err);
    bool finalizeActiveFile(quint32 fileIndex, QString* err);

    QTcpSocket* sock_ = nullptr;
    QByteArray rx_;
    bool running_ = false;
    bool finishing_ = false;
    bool canceling_ = false;

    QString host_;
    quint16 port_ = 0;
    QString transferId_;
    QString type_;
    QString cacheRoot_;

    QVector<ExpectedFile> expected_;
    QVector<ActiveFile> active_;
    QHash<QString, int> expectedNameToIndex_;
    QSet<int> completedIndexes_;
    QStringList completedRecordIds_;
    quint64 totalBytes_ = 0;
    quint64 receivedBytes_ = 0;
    QElapsedTimer progressTimer_;
};

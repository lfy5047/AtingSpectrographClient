#include "RemoteFileDownloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTcpSocket>
#include <QtEndian>
#include <limits>
#include <QNetworkProxy>

#include "nlohmann/json.hpp"
#include "plog/Log.h"

namespace {
const quint32 kFileMagic = 0x44465441u; // 'ATFD' LE
const quint16 kFileVersion = 1;
const quint16 kLastChunk = 0x1;
const quint16 kLastFile = 0x2;
const int kHeaderBytes = 44;

quint32 u32(const char* p) { return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(p)); }
quint16 u16(const char* p) { return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(p)); }
quint64 u64(const char* p) { return qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(p)); }

bool removeDirRecursive(const QString& path)
{
    QDir dir(path);
    return !dir.exists() || dir.removeRecursively();
}
}

RemoteFileDownloader::RemoteFileDownloader(QObject* parent)
    : QObject(parent)
{
}

RemoteFileDownloader::~RemoteFileDownloader()
{
    cancel();
}

void RemoteFileDownloader::start(const QString& host, quint16 port, const QString& transferId,
                                 const QString& type, const QVector<RemoteFetchFile>& files,
                                 const QString& cacheRoot)
{
    if (running_) {
        fail(RemoteDownloadErrorReason::Unknown, QString::fromUtf8("已有下载任务正在进行"));
        return;
    }

    host_ = host;
    port_ = port;
    transferId_ = transferId;
    type_ = type;
    cacheRoot_ = cacheRoot;
    rx_.clear();
    completedIndexes_.clear();
    completedRecordIds_.clear();
    totalBytes_ = 0;
    receivedBytes_ = 0;
    finishing_ = false;
    canceling_ = false;
    progressTimer_.invalidate();

    QString err;
    if (!prepareExpectedFiles(files, &err) || !prepareDirectories(&err)) {
        fail(RemoteDownloadErrorReason::FileIo, err);
        return;
    }

    sock_ = new QTcpSocket(this);
    sock_->setProxy(QNetworkProxy::NoProxy);
    connect(sock_, &QTcpSocket::connected, this, &RemoteFileDownloader::onConnected);
    connect(sock_, &QTcpSocket::readyRead, this, &RemoteFileDownloader::onReadyRead);
    connect(sock_, &QTcpSocket::disconnected, this, &RemoteFileDownloader::onDisconnected);
    connect(sock_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &RemoteFileDownloader::onError);

    running_ = true;
    sock_->connectToHost(host_, port_);
    LOGD << "连接到: " << host_.toStdString() << ":" << port_;
}

void RemoteFileDownloader::cancel()
{
    if (!running_) return;
    canceling_ = true;
    cleanupSocket();
    cleanupOpenFile();
    cleanupTempFiles();
    running_ = false;
    emit canceled();
}

bool RemoteFileDownloader::prepareExpectedFiles(const QVector<RemoteFetchFile>& files, QString* err)
{
    expected_.clear();
    active_.clear();
    expectedNameToIndex_.clear();

    if (files.isEmpty()) {
        if (err) *err = QString::fromUtf8("没有可下载文件");
        return false;
    }

    QSet<QString> names;
    for (const RemoteFetchFile& f : files) {
        if (f.recordId.isEmpty() || f.name.isEmpty() || f.sizeBytes == 0) {
            if (err) *err = QString::fromUtf8("record.fetch 返回了无效文件信息");
            return false;
        }
        if (!isSafeFileName(f.name)) {
            if (err) *err = QString::fromUtf8("服务端返回了非法文件名: %1").arg(f.name);
            return false;
        }
        if (names.contains(f.name)) {
            if (err) *err = QString::fromUtf8("本次下载存在重复文件名: %1").arg(f.name);
            return false;
        }
        names.insert(f.name);

        ExpectedFile ef;
        ef.recordId = f.recordId;
        ef.name = f.name;
        ef.sizeBytes = f.sizeBytes;
        const QString dir = QDir(cacheRoot_).filePath(type_ + "/" + f.recordId);
        ef.tempPath = QDir(dir).filePath(f.name + ".part");
        ef.finalPath = QDir(dir).filePath(f.name);
        expectedNameToIndex_.insert(f.name, expected_.size());
        expected_.append(ef);
        totalBytes_ += f.sizeBytes;
    }
    active_.resize(expected_.size());
    return true;
}

bool RemoteFileDownloader::prepareDirectories(QString* err)
{
    QSet<QString> dirs;
    for (const ExpectedFile& f : expected_) {
        const QString dir = QFileInfo(f.finalPath).absolutePath();
        if (dirs.contains(dir)) continue;
        dirs.insert(dir);
        if (!removeDirRecursive(dir)) {
            if (err) *err = QString::fromUtf8("无法清理缓存目录: %1").arg(dir);
            return false;
        }
        if (!QDir().mkpath(dir)) {
            if (err) *err = QString::fromUtf8("无法创建缓存目录: %1").arg(dir);
            return false;
        }
    }
    return true;
}

void RemoteFileDownloader::onConnected()
{
    nlohmann::json body = {{"transfer_id", transferId_.toStdString()}};
    const QByteArray json = QByteArray::fromStdString(body.dump());
    QByteArray packet;
    packet.resize(4);
    qToLittleEndian<quint32>(static_cast<quint32>(json.size()), reinterpret_cast<uchar*>(packet.data()));
    packet.append(json);
    sock_->write(packet);
}

void RemoteFileDownloader::onReadyRead()
{
    if (!sock_) return;
    rx_.append(sock_->readAll());
    QString err;
    if (!consumeAvailable(&err)) {
        fail(RemoteDownloadErrorReason::Protocol, err);
    }
}

void RemoteFileDownloader::onDisconnected()
{
    if (!running_) return;
    if (canceling_) return;
    if (finishing_ && allExpectedFilesComplete()) {
        complete();
        return;
    }
    fail(RemoteDownloadErrorReason::Network, QString::fromUtf8("文件传输连接已断开"));
}

void RemoteFileDownloader::onError()
{
    if (!running_ || canceling_) return;
    const QString err = sock_ ? sock_->errorString() : QString::fromUtf8("文件传输失败");
    fail(RemoteDownloadErrorReason::Network, err);
}

bool RemoteFileDownloader::consumeAvailable(QString* err)
{
    while (rx_.size() >= kHeaderBytes) {
        bool consumed = false;
        if (!consumeOneChunk(err, &consumed)) {
            LOGD << "处理数据块失败: " << (err ? err->toStdString() : "未知错误");
            return false;
        }
        if (!consumed) break;
    }
    return true;
}

bool RemoteFileDownloader::consumeOneChunk(QString* err, bool* consumed)
{
    if (consumed) *consumed = false;
    const char* h = rx_.constData();
    const quint32 magic = u32(h + 0);
    const quint16 version = u16(h + 4);
    const quint16 flags = u16(h + 6);
    const quint32 fileIndex = u32(h + 8);
    const quint32 fileCount = u32(h + 12);
    const quint64 offset = u64(h + 16);
    const quint64 fileSize = u64(h + 24);
    const quint32 filenameLen = u32(h + 32);
    const quint32 payloadLen = u32(h + 36);
    const quint32 reserved = u32(h + 40);

    if (magic != kFileMagic) {
        if (err) *err = QString::fromUtf8("文件传输 magic 不匹配");
        return false;
    }
    if (version != kFileVersion) {
        if (err) *err = QString::fromUtf8("文件传输版本不支持");
        return false;
    }
    if (reserved != 0) {
        if (err) *err = QString::fromUtf8("文件传输 reserved 非零");
        return false;
    }
    if (fileCount != static_cast<quint32>(expected_.size()) || fileIndex >= fileCount) {
        if (err) *err = QString::fromUtf8("文件序号或文件数量不匹配");
        return false;
    }
    if (filenameLen == 0 || filenameLen > 4096) {
        if (err) *err = QString::fromUtf8("文件名长度非法");
        return false;
    }

    const quint64 needed64 = static_cast<quint64>(kHeaderBytes) + filenameLen + payloadLen;
    if (needed64 > static_cast<quint64>(std::numeric_limits<int>::max())) {
        if (err) *err = QString::fromUtf8("文件块过大");
        return false;
    }
    const int needed = static_cast<int>(needed64);
    if (rx_.size() < needed) return true;

    const QByteArray nameBytes = rx_.mid(kHeaderBytes, static_cast<int>(filenameLen));
    const QString name = QString::fromUtf8(nameBytes);
    const QByteArray payload = rx_.mid(kHeaderBytes + static_cast<int>(filenameLen),
                                       static_cast<int>(payloadLen));
    rx_.remove(0, needed);
    if (consumed) *consumed = true;

    if (!isSafeFileName(name) || !expectedNameToIndex_.contains(name)) {
        if (err) *err = QString::fromUtf8("收到非预期文件名: %1").arg(name);
        return false;
    }
    const ExpectedFile& expected = expected_[static_cast<int>(fileIndex)];
    if (expected.name != name || expected.sizeBytes != fileSize) {
        if (err) *err = QString::fromUtf8("文件 header 与 fetch 响应不一致: %1").arg(name);
        return false;
    }

    if (!openActiveFile(fileIndex, name, fileSize, err)) return false;
    if (!writePayload(fileIndex, offset, payload, err)) return false;

    const bool lastChunk = (flags & kLastChunk) != 0;
    const bool lastFile = (flags & kLastFile) != 0;
    if (lastChunk) {
        if (offset + payloadLen != fileSize) {
            if (err) *err = QString::fromUtf8("LastChunk 偏移不等于文件大小");
            return false;
        }
        if (!finalizeActiveFile(fileIndex, err)) return false;
    }
    if (lastFile) {
        if (!allExpectedFilesComplete()) {
            if (err) *err = QString::fromUtf8("LastFile 早于所有文件完成");
            return false;
        }
        finishing_ = true;
        if (sock_) sock_->disconnectFromHost();
    }
    // LOGD << "接收进度: " << receivedBytes_ << " / " << totalBytes_ << " (" << name.toStdString() << ")";
    if (!progressTimer_.isValid() || progressTimer_.elapsed() >= 100 || lastChunk || lastFile) {
        progressTimer_.restart();
        emit progress(receivedBytes_, totalBytes_, name);
    }
    return true;
}

bool RemoteFileDownloader::isSafeFileName(const QString& name) const
{
    if (name.isEmpty()) return false;
    QFileInfo info(name);
    if (info.isAbsolute()) return false;
    return !name.contains("..") && !name.contains('/') && !name.contains('\\');
}

bool RemoteFileDownloader::openActiveFile(quint32 fileIndex, const QString& name,
                                          quint64 fileSize, QString* err)
{
    ActiveFile& af = active_[static_cast<int>(fileIndex)];
    if (af.seen) {
        if (af.name != name || af.fileSize != fileSize) {
            if (err) *err = QString::fromUtf8("同一 file_index 的文件信息发生变化");
            return false;
        }
        return true;
    }

    af.seen = true;
    af.name = name;
    af.fileSize = fileSize;
    af.received = 0;
    af.file = new QFile(expected_[static_cast<int>(fileIndex)].tempPath);
    if (!af.file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QString::fromUtf8("无法写入缓存文件: %1").arg(af.file->fileName());
        delete af.file;
        af.file = nullptr;
        return false;
    }
    return true;
}

bool RemoteFileDownloader::writePayload(quint32 fileIndex, quint64 offset,
                                        const QByteArray& payload, QString* err)
{
    ActiveFile& af = active_[static_cast<int>(fileIndex)];
    if (!af.file) {
        if (err) *err = QString::fromUtf8("文件未打开");
        return false;
    }
    if (offset != af.received) {
        if (err) *err = QString::fromUtf8("文件 offset 不连续");
        return false;
    }
    if (offset + static_cast<quint64>(payload.size()) > af.fileSize) {
        if (err) *err = QString::fromUtf8("文件 payload 超出声明大小");
        return false;
    }
    if (af.file->write(payload) != payload.size()) {
        if (err) *err = QString::fromUtf8("写入缓存文件失败");
        return false;
    }
    af.received += static_cast<quint64>(payload.size());
    receivedBytes_ += static_cast<quint64>(payload.size());
    return true;
}

bool RemoteFileDownloader::finalizeActiveFile(quint32 fileIndex, QString* err)
{
    ActiveFile& af = active_[static_cast<int>(fileIndex)];
    if (!af.file || af.received != af.fileSize) {
        if (err) *err = QString::fromUtf8("文件未完整接收");
        return false;
    }
    af.file->close();
    delete af.file;
    af.file = nullptr;

    const ExpectedFile& ef = expected_[static_cast<int>(fileIndex)];
    QFile::remove(ef.finalPath);
    if (!QFile::rename(ef.tempPath, ef.finalPath)) {
        if (err) *err = QString::fromUtf8("缓存文件提交失败: %1").arg(ef.name);
        return false;
    }
    completedIndexes_.insert(static_cast<int>(fileIndex));
    if (!completedRecordIds_.contains(ef.recordId)) {
        completedRecordIds_.append(ef.recordId);
    }
    return true;
}

bool RemoteFileDownloader::allExpectedFilesComplete() const
{
    return completedIndexes_.size() == expected_.size();
}

void RemoteFileDownloader::complete()
{
    cleanupSocket();
    cleanupOpenFile();
    running_ = false;
    emit finished(completedRecordIds_);
}

void RemoteFileDownloader::fail(RemoteDownloadErrorReason reason, const QString& err)
{
    const bool wasRunning = running_;
    cleanupSocket();
    cleanupOpenFile();
    cleanupTempFiles();
    running_ = false;
    if (wasRunning || !err.isEmpty()) {
        emit failed(reason, err.isEmpty() ? QString::fromUtf8("文件下载失败") : err);
    }
}

void RemoteFileDownloader::cleanupSocket()
{
    if (!sock_) return;
    sock_->disconnect(this);
    sock_->abort();
    sock_->deleteLater();
    sock_ = nullptr;
}

void RemoteFileDownloader::cleanupOpenFile()
{
    for (ActiveFile& af : active_) {
        if (!af.file) continue;
        af.file->close();
        delete af.file;
        af.file = nullptr;
    }
}

void RemoteFileDownloader::cleanupTempFiles()
{
    QSet<QString> dirs;
    for (int i = 0; i < expected_.size(); ++i) {
        const ExpectedFile& ef = expected_[i];
        QFile::remove(ef.tempPath);
        dirs.insert(QFileInfo(ef.finalPath).absolutePath());
    }
    for (const QString& dir : dirs) {
        removeDirRecursive(dir);
    }
}

#include "LocalRecordScanner.h"

#include "nlohmann/json.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <limits>

namespace {

QString jsonString(const nlohmann::json& j, const char* key)
{
    return j.contains(key) && j[key].is_string()
        ? QString::fromStdString(j[key].get<std::string>())
        : QString();
}

quint64 jsonU64(const nlohmann::json& j, const char* key, quint64 def = 0)
{
    if (!j.contains(key)) return def;
    if (j[key].is_number_unsigned()) return j[key].get<quint64>();
    if (j[key].is_number_integer()) {
        const qint64 v = j[key].get<qint64>();
        return v >= 0 ? static_cast<quint64>(v) : def;
    }
    if (j[key].is_string()) {
        bool ok = false;
        const quint64 v = QString::fromStdString(j[key].get<std::string>()).toULongLong(&ok);
        return ok ? v : def;
    }
    return def;
}

int jsonInt(const nlohmann::json& j, const char* key, int def = 0)
{
    if (!j.contains(key) || !j[key].is_number_integer()) return def;
    return j[key].get<int>();
}

bool checkedMul(quint64 a, quint64 b, quint64* out)
{
    if (!out) return false;
    if (a != 0 && b > std::numeric_limits<quint64>::max() / a) return false;
    *out = a * b;
    return true;
}

bool parseRecordId(const QString& id, quint64* out)
{
    bool ok = false;
    const quint64 value = id.toULongLong(&ok, 10);
    if (!ok) return false;
    if (out) *out = value;
    return true;
}

bool appendFileIfValid(const QDir& dir, const QString& name, quint64 expectedSize,
                       QVector<LocalRecordScanner::File>* files)
{
    if (name.isEmpty() || name.contains('/') || name.contains('\\') || name == "." || name == "..") {
        return false;
    }
    const QFileInfo info(dir.filePath(name));
    if (!info.exists() || !info.isFile()) return false;
    const quint64 actualSize = static_cast<quint64>(qMax<qint64>(0, info.size()));
    if (actualSize == 0) return false;
    if (expectedSize > 0 && actualSize != expectedSize) return false;
    LocalRecordScanner::File file;
    file.name = name;
    file.sizeBytes = actualSize;
    files->append(file);
    return true;
}

bool readManifestRecord(const QDir& recordDir, const QString& rootPath, const QString& type,
                        const QString& recordId, quint64 recordIdValue,
                        LocalRecordScanner::Record* out)
{
    QFile file(recordDir.filePath("manifest.json"));
    if (!file.open(QIODevice::ReadOnly)) return false;

    nlohmann::json manifest;
    try {
        manifest = nlohmann::json::parse(file.readAll().constData());
    } catch (...) {
        return false;
    }

    const QString manifestType = jsonString(manifest, "type");
    const QString manifestId = jsonString(manifest, "record_id");
    if ((!manifestType.isEmpty() && manifestType != type) ||
        (!manifestId.isEmpty() && manifestId != recordId) ||
        !manifest.contains("files") || !manifest["files"].is_array()) {
        return false;
    }

    LocalRecordScanner::Record record;
    record.rootPath = rootPath;
    record.type = type;
    record.recordId = recordId;
    record.recordIdValue = recordIdValue;
    record.timestampNs = jsonU64(manifest, "timestamp_ns", 0);
    for (const auto& jf : manifest["files"]) {
        if (!appendFileIfValid(recordDir, jsonString(jf, "name"),
                               jsonU64(jf, "size_bytes", 0), &record.files)) {
            return false;
        }
    }
    if (record.files.isEmpty()) return false;
    *out = record;
    return true;
}

bool readRawShape(const QString& jsonPath, quint64* expectedBytes)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    nlohmann::json meta;
    try {
        meta = nlohmann::json::parse(file.readAll().constData());
    } catch (...) {
        return false;
    }
    const int width = jsonInt(meta, "width");
    const int height = jsonInt(meta, "height");
    const quint64 frameCount = jsonU64(meta, "frame_count", 0);
    if (width <= 0 || height <= 0 || frameCount == 0) return false;
    if (!meta.contains("frames") || !meta["frames"].is_array() ||
        meta["frames"].size() < static_cast<std::size_t>(frameCount)) {
        return false;
    }
    quint64 pixels = 0;
    quint64 bytesPerFrame = 0;
    return checkedMul(static_cast<quint64>(width), static_cast<quint64>(height), &pixels) &&
           checkedMul(pixels, 2, &bytesPerFrame) &&
           checkedMul(bytesPerFrame, frameCount, expectedBytes);
}

bool inferRawRecord(const QDir& recordDir, const QString& rootPath,
                    const QString& recordId, quint64 recordIdValue,
                    LocalRecordScanner::Record* out)
{
    const QFileInfoList files = recordDir.entryInfoList(QDir::Files, QDir::Name);
    QString jsonName;
    QString rawName;
    for (const QFileInfo& file : files) {
        if (file.fileName().endsWith(".json", Qt::CaseInsensitive) && file.fileName() != "manifest.json") {
            jsonName = file.fileName();
        } else if (file.fileName().endsWith(".raw", Qt::CaseInsensitive)) {
            rawName = file.fileName();
        }
    }
    if (jsonName.isEmpty() || rawName.isEmpty()) return false;

    quint64 expectedBytes = 0;
    if (!readRawShape(recordDir.filePath(jsonName), &expectedBytes)) return false;

    LocalRecordScanner::Record record;
    record.rootPath = rootPath;
    record.type = "raw";
    record.recordId = recordId;
    record.recordIdValue = recordIdValue;
    if (!appendFileIfValid(recordDir, rawName, expectedBytes, &record.files)) return false;
    if (!appendFileIfValid(recordDir, jsonName, 0, &record.files)) return false;
    *out = record;
    return true;
}

bool inferTifRecord(const QDir& recordDir, const QString& rootPath,
                    const QString& recordId, quint64 recordIdValue,
                    LocalRecordScanner::Record* out)
{
    const QFileInfoList files = recordDir.entryInfoList(QDir::Files, QDir::Name);
    LocalRecordScanner::Record record;
    record.rootPath = rootPath;
    record.type = "tif";
    record.recordId = recordId;
    record.recordIdValue = recordIdValue;
    for (const QFileInfo& file : files) {
        if (file.fileName().endsWith(".tif", Qt::CaseInsensitive) ||
            file.fileName().endsWith(".tiff", Qt::CaseInsensitive)) {
            appendFileIfValid(recordDir, file.fileName(), 0, &record.files);
        }
    }
    if (record.files.isEmpty()) return false;
    *out = record;
    return true;
}

} // namespace

QVector<LocalRecordScanner::Record> LocalRecordScanner::scan(const QString& rootPath,
                                                             const QString& type,
                                                             QString* err)
{
    if (err) err->clear();
    QVector<Record> records;
    if (type != "raw" && type != "tif") {
        if (err) *err = QString::fromUtf8("不支持的本地记录类型: %1").arg(type);
        return records;
    }

    const QDir root(rootPath);
    if (!root.exists()) {
        if (err) *err = QString::fromUtf8("本地记录目录不存在: %1").arg(rootPath);
        return records;
    }

    const QString normalizedRoot = root.absolutePath();
    const QDir typeDir(root.filePath(type));
    if (!typeDir.exists()) return records;

    const QFileInfoList dirs = typeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& dirInfo : dirs) {
        quint64 recordIdValue = 0;
        if (!parseRecordId(dirInfo.fileName(), &recordIdValue)) continue;

        const QDir recordDir(dirInfo.absoluteFilePath());
        Record record;
        if (!readManifestRecord(recordDir, normalizedRoot, type, dirInfo.fileName(), recordIdValue, &record)) {
            if (type == "raw") {
                if (!inferRawRecord(recordDir, normalizedRoot, dirInfo.fileName(), recordIdValue, &record)) continue;
            } else {
                if (!inferTifRecord(recordDir, normalizedRoot, dirInfo.fileName(), recordIdValue, &record)) continue;
            }
        }
        records.append(record);
    }

    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
        return a.recordIdValue < b.recordIdValue;
    });
    return records;
}

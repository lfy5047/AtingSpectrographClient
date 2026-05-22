#include "FramePlaybackScanWorker.h"

#include <QFile>
#include <QMetaObject>

FramePlaybackScanWorker::FramePlaybackScanWorker(FramePlaybackController* owner, QObject* parent)
    : QObject(parent), owner_(owner)
{}

void FramePlaybackScanWorker::scan(const QString& path)
{
    recording::FileHeader header;
    QList<FramePlaybackController::IndexEntry> idx;
    quint64 damaged = 0;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        postResult(path, false, f.errorString(), header, idx, damaged);
        return;
    }

    QString err;
    if (!recording::readFileHeader(f, header, &err)) {
        postResult(path, false, err, header, idx, damaged);
        return;
    }

    for (;;) {
        if (f.atEnd()) break;

        const qint64 headerPos = f.pos();
        recording::FrameHeader fh;
        QString ferr;
        if (!recording::readFrameHeader(f, fh, &ferr)) {
            // EOF is okay: clean end. Any other read failure is treated as truncation.
            if (f.atEnd()) break;
            postResult(path, false, ferr, header, idx, damaged);
            return;
        }

        if (fh.dataBytes > 256u * 1024u * 1024u) {
            postResult(path, false, "payload too large", header, idx, damaged);
            return;
        }

        FramePlaybackController::IndexEntry e;
        e.streamFrameId = fh.frameIndex;
        e.timestampMs = fh.timestampMs;
        e.frameHeaderOffset = static_cast<quint64>(headerPos);
        e.payloadOffset = static_cast<quint64>(f.pos());
        e.payloadBytes = fh.dataBytes;
        e.crc32 = fh.crc32;
        e.frameType = fh.reserved8;
        e.channel = fh.channel;
        e.width = fh.width;
        e.height = fh.height;
        e.pixfmt = fh.pixfmt;
        idx.append(e);

        // Seek to next frame payload end
        if (!f.seek(f.pos() + fh.dataBytes)) {
            postResult(path, false, "truncated payload", header, idx, damaged);
            return;
        }

        Q_UNUSED(headerPos);
    }

    // Recover header stats if needed
    if (header.frameCount == 0 || (header.flags & recording::FileFlagClosedOk) == 0) {
        header.frameCount = static_cast<quint64>(idx.size());
        if (!idx.isEmpty()) header.durationMs = idx.last().timestampMs;
    }

    postResult(path, true, QString(), header, idx, damaged);
}

void FramePlaybackScanWorker::postResult(const QString& path, bool ok, const QString& err,
                                         const recording::FileHeader& header,
                                         const QList<FramePlaybackController::IndexEntry>& idx,
                                         quint64 damaged)
{
    FramePlaybackController* owner = owner_;
    if (!owner) return;
    QMetaObject::invokeMethod(owner, [owner, path, ok, err, header, idx, damaged]() {
        owner->scanning_ = false;
        if (!ok) {
            owner->indexReady_ = false;
            owner->path_ = path;
            owner->header_ = header;
            owner->index_.clear();
            owner->damagedFrames_ = damaged;
            owner->damagedIndexSet_.clear();
            emit owner->scanFinished(path, false, err);
            return;
        }

        owner->path_ = path;
        owner->header_ = header;
        owner->index_ = idx;
        owner->damagedFrames_ = damaged;
        owner->damagedIndexSet_.clear();
        owner->indexReady_ = true;
        emit owner->indexReady();
        emit owner->scanFinished(path, true, QString());
    }, Qt::QueuedConnection);
}

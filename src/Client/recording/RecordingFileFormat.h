#pragma once

#include <QtGlobal>
#include <QString>

class QDataStream;
class QIODevice;
class QByteArray;

namespace recording {

static const quint32 kAsrecFileMagic  = 0x43525341u; // 'ASRC' LE
static const quint32 kAsrecFrameMagic = 0x4D524641u; // 'AFRM' LE
static const quint32 kAsrecVersion    = 1u;

static const quint32 kAsrecFileHeaderSize  = 64u;
static const quint32 kAsrecFrameHeaderSize = 56u;

enum FileFlags : quint32 {
    FileFlagClosedOk = 1u << 0, // 1 = normal stop + header rewritten, 0 = possible crash/truncation
};

struct FileHeader {
    quint32 magic = kAsrecFileMagic;
    quint32 version = kAsrecVersion;
    quint32 headerSize = kAsrecFileHeaderSize;
    quint64 createdUnixMs = 0;
    quint64 frameCount = 0;
    quint64 durationMs = 0;
    quint32 flags = 0;
};

struct FrameHeader {
    quint32 magic = kAsrecFrameMagic;
    quint32 headerSize = kAsrecFrameHeaderSize;
    quint8  channel = 0;
    quint8  reserved8 = 0;
    quint16 width = 0;
    quint16 height = 0;
    quint16 pixfmt = 0;
    quint16 reserved16 = 0;
    quint64 frameIndex = 0;
    quint64 timestampMs = 0;
    quint32 dataBytes = 0;
    quint32 crc32 = 0;
    quint32 reserved32 = 0;
};

void configureStream(QDataStream& s);

bool writeFileHeader(QIODevice& dev, const FileHeader& h);
bool rewriteFileHeader(QIODevice& dev, const FileHeader& h);
bool readFileHeader(QIODevice& dev, FileHeader& out, QString* err = nullptr);

bool writeFrameHeader(QIODevice& dev, const FrameHeader& h);
bool readFrameHeader(QIODevice& dev, FrameHeader& out, QString* err = nullptr);

quint32 crc32_ieee(const QByteArray& data);

} // namespace recording

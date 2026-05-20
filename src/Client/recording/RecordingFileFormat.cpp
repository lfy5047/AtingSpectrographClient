#include "RecordingFileFormat.h"

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include "plog/Log.h"

namespace recording {

void configureStream(QDataStream& s)
{
    s.setByteOrder(QDataStream::LittleEndian);
    s.setVersion(QDataStream::Qt_5_15);
}

static bool writeReserved32(QDataStream& s, int count)
{
    for (int i = 0; i < count; ++i) {
        s << static_cast<quint32>(0);
        if (s.status() != QDataStream::Ok) return false;
    }
    return true;
}

bool writeFileHeader(QIODevice& dev, const FileHeader& h)
{
    if (!dev.isWritable()) return false;
    QDataStream s(&dev);
    configureStream(s);

    s << h.magic
      << h.version
      << h.headerSize
      << h.createdUnixMs
      << h.frameCount
      << h.durationMs
      << h.flags;

    if (s.status() != QDataStream::Ok) return false;
    // 24 bytes reserved = 6x quint32
    return writeReserved32(s, 6);
}

bool rewriteFileHeader(QIODevice& dev, const FileHeader& h)
{
    if (!dev.isWritable()) return false;
    if (!dev.isSequential() && !dev.seek(0)) return false;
    return writeFileHeader(dev, h);
}

bool readFileHeader(QIODevice& dev, FileHeader& out, QString* err)
{
    if (!dev.isReadable()) return false;
    if (!dev.isSequential() && !dev.seek(0)) {
        if (err) *err = "seek(0) failed";
        return false;
    }

    QDataStream s(&dev);
    configureStream(s);

    quint32 magic = 0, version = 0, headerSize = 0, flags = 0;
    quint64 createdUnixMs = 0, frameCount = 0, durationMs = 0;
    s >> magic >> version >> headerSize >> createdUnixMs >> frameCount >> durationMs >> flags;
    if (s.status() != QDataStream::Ok) {
        if (err) *err = "read header failed";
        LOGE << "read header failed: " << s.status();
        return false;
    }

    // consume reserved
    for (int i = 0; i < 6; ++i) {
        quint32 tmp = 0;
        s >> tmp;
        if (s.status() != QDataStream::Ok) {
            if (err) *err = "read header reserved failed";
            LOGE << "read header reserved failed: " << s.status();
            return false;
        }
    }

    if (magic != kAsrecFileMagic) {
        if (err) *err = "bad magic";
        LOGE << "bad magic: " << magic;
        return false;
    }
    if (version != kAsrecVersion) {
        if (err) *err = "unsupported version";
        LOGE << "unsupported version: " << version;
        return false;
    }
    if (headerSize != kAsrecFileHeaderSize) {
        if (err) *err = "bad header size";
        LOGE << "bad header size: " << headerSize;
        return false;
    }

    out.magic = magic;
    out.version = version;
    out.headerSize = headerSize;
    out.createdUnixMs = createdUnixMs;
    out.frameCount = frameCount;
    out.durationMs = durationMs;
    out.flags = flags;
    return true;
}

bool writeFrameHeader(QIODevice& dev, const FrameHeader& h)
{
    if (!dev.isWritable()) return false;
    QDataStream s(&dev);
    configureStream(s);

    s << h.magic
      << h.headerSize
      << h.channel
      << h.reserved8
      << h.width
      << h.height
      << h.pixfmt
      << h.reserved16
      << h.frameIndex
      << h.timestampMs
      << h.dataBytes
      << h.crc32
      << h.reserved32;

    return s.status() == QDataStream::Ok;
}

bool readFrameHeader(QIODevice& dev, FrameHeader& out, QString* err)
{
    if (!dev.isReadable()) return false;
    QDataStream s(&dev);
    configureStream(s);

    quint32 magic = 0, headerSize = 0;
    quint8 channel = 0, reserved8 = 0;
    quint16 width = 0, height = 0, pixfmt = 0, reserved16 = 0;
    quint64 frameIndex = 0, timestampMs = 0;
    quint32 dataBytes = 0, crc32 = 0, reserved32 = 0;

    s >> magic
      >> headerSize
      >> channel
      >> reserved8
      >> width
      >> height
      >> pixfmt
      >> reserved16
      >> frameIndex
      >> timestampMs
      >> dataBytes
      >> crc32
      >> reserved32;

    if (s.status() != QDataStream::Ok) {
        if (err) *err = "read frame header failed";
        LOGE << "read frame header failed: " << s.status();
        return false;
    }

    if (magic != kAsrecFrameMagic) {
        if (err) *err = "bad frame magic";
        LOGE << "bad frame magic: " << magic;
        return false;
    }
    if (headerSize != kAsrecFrameHeaderSize) {
        if (err) *err = "bad frame header size";
        LOGE << "bad frame header size: " << headerSize;
        return false;
    }

    out.magic = magic;
    out.headerSize = headerSize;
    out.channel = channel;
    out.reserved8 = reserved8;
    out.width = width;
    out.height = height;
    out.pixfmt = pixfmt;
    out.reserved16 = reserved16;
    out.frameIndex = frameIndex;
    out.timestampMs = timestampMs;
    out.dataBytes = dataBytes;
    out.crc32 = crc32;
    out.reserved32 = reserved32;
    return true;
}

quint32 crc32_ieee(const QByteArray& data)
{
    static quint32 table[256];
    static bool inited = false;
    if (!inited) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        inited = true;
    }

    quint32 crc = 0xFFFFFFFFu;
    const uchar* p = reinterpret_cast<const uchar*>(data.constData());
    int n = data.size();
    for (int i = 0; i < n; ++i) {
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace recording


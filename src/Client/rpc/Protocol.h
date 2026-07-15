#pragma once

#include <cstddef>
#include <cstdint>

namespace cli { namespace proto {

static const uint32_t kCtrlMagic   = 0x4E495441u;  // 'ATIN' LE
static const uint32_t kStreamMagic = 0x4D545341u;  // 'ASTM' LE
static const uint16_t kCtrlProtoVersion = 2;
static const uint8_t kStreamProtoVersion = 4;
static const uint8_t kStreamMetadataProtoVersion = 3;
static const uint32_t kMaxCtrlPayload = 1u << 20;  // 1 MiB

enum MsgType : uint16_t {
    Request  = 1,
    Response = 2,
    Event    = 3,
    Error    = 4,
};

enum StreamChannel : uint8_t {
    Raw16          = 1,
    Preview8       = 2,
    SliceStitch16  = 3,
    RegionStitch16 = 4,
    NucRaw16       = 5,
    SpectralPreview = 6,
};

enum PixelFormat : uint16_t {
    Mono8  = 1,
    Mono16 = 2,
    Jpeg   = 3,
};

enum StreamFlag : uint16_t {
    LastFragment = 0x1,
};

enum StreamMetaFlag : uint16_t {
    HasRawFrameMeta = 0x1,
    HasScanDirection = 0x2,
};

enum StreamFrameType : uint8_t {
    UnknownFrame = 0,
    HeaderFrame = 1,
    DataFrame = 2,
    TailFrame = 3,
};

#pragma pack(push, 1)
struct CtrlHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t seq;
    uint32_t payload_len;
};

struct StreamHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  channel;
    uint16_t flags;
    uint64_t frame_id;
    uint16_t pkt_index;
    uint16_t pkt_total;
    uint32_t frame_bytes;
    uint16_t width;
    uint16_t height;
    uint16_t pixel_format;
    uint16_t meta_flags;
    int64_t  timestamp_ns;
    int64_t  mirror_timestamp_ns;
    uint64_t mirror_angle_bits;
    uint8_t  frame_type;
    uint8_t  is_latest_mirror_frame;
    uint8_t  reverse_scan;
    uint8_t  reserved0;
    uint32_t reserved1;
};
#pragma pack(pop)

static_assert(sizeof(CtrlHeader)   == 16, "CtrlHeader must be 16 bytes");
static_assert(sizeof(StreamHeader) == 64, "StreamHeader must be 64 bytes");
static_assert(offsetof(StreamHeader, magic) == 0, "StreamHeader.magic offset mismatch");
static_assert(offsetof(StreamHeader, version) == 4, "StreamHeader.version offset mismatch");
static_assert(offsetof(StreamHeader, channel) == 5, "StreamHeader.channel offset mismatch");
static_assert(offsetof(StreamHeader, flags) == 6, "StreamHeader.flags offset mismatch");
static_assert(offsetof(StreamHeader, frame_id) == 8, "StreamHeader.frame_id offset mismatch");
static_assert(offsetof(StreamHeader, pkt_index) == 16, "StreamHeader.pkt_index offset mismatch");
static_assert(offsetof(StreamHeader, pkt_total) == 18, "StreamHeader.pkt_total offset mismatch");
static_assert(offsetof(StreamHeader, frame_bytes) == 20, "StreamHeader.frame_bytes offset mismatch");
static_assert(offsetof(StreamHeader, width) == 24, "StreamHeader.width offset mismatch");
static_assert(offsetof(StreamHeader, height) == 26, "StreamHeader.height offset mismatch");
static_assert(offsetof(StreamHeader, pixel_format) == 28, "StreamHeader.pixel_format offset mismatch");
static_assert(offsetof(StreamHeader, meta_flags) == 30, "StreamHeader.meta_flags offset mismatch");
static_assert(offsetof(StreamHeader, timestamp_ns) == 32, "StreamHeader.timestamp_ns offset mismatch");
static_assert(offsetof(StreamHeader, mirror_timestamp_ns) == 40, "StreamHeader.mirror_timestamp_ns offset mismatch");
static_assert(offsetof(StreamHeader, mirror_angle_bits) == 48, "StreamHeader.mirror_angle_bits offset mismatch");
static_assert(offsetof(StreamHeader, frame_type) == 56, "StreamHeader.frame_type offset mismatch");
static_assert(offsetof(StreamHeader, is_latest_mirror_frame) == 57, "StreamHeader.is_latest_mirror_frame offset mismatch");
static_assert(offsetof(StreamHeader, reverse_scan) == 58, "StreamHeader.reverse_scan offset mismatch");
static_assert(offsetof(StreamHeader, reserved0) == 59, "StreamHeader.reserved0 offset mismatch");
static_assert(offsetof(StreamHeader, reserved1) == 60, "StreamHeader.reserved1 offset mismatch");

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "Stream protocol currently supports little-endian hosts only."
#endif

inline uint32_t channelBit(StreamChannel ch) {
    return 1u << static_cast<uint8_t>(ch);
}

}} // namespace cli::proto

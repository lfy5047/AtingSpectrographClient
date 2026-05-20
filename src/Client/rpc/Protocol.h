#pragma once

#include <cstdint>

namespace cli { namespace proto {

static const uint32_t kCtrlMagic   = 0x4E495441u;  // 'ATIN' LE
static const uint32_t kStreamMagic = 0x4D545341u;  // 'ASTM' LE
static const uint16_t kProtoVersion = 1;
static const uint32_t kMaxCtrlPayload = 1u << 20;  // 1 MiB

enum MsgType : uint16_t {
    Request  = 1,
    Response = 2,
    Event    = 3,
    Error    = 4,
};

enum StreamChannel : uint8_t {
    Raw16          = 1,
    SliceStitch16  = 3,
};

enum PixelFormat : uint16_t {
    Mono8  = 1,
    Mono16 = 2,
};

enum StreamFlag : uint16_t {
    LastFragment = 0x1,
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
    uint16_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(CtrlHeader)   == 16, "CtrlHeader must be 16 bytes");
static_assert(sizeof(StreamHeader) == 32, "StreamHeader must be 32 bytes");

inline uint32_t channelBit(StreamChannel ch) {
    return 1u << static_cast<uint8_t>(ch);
}

}} // namespace cli::proto

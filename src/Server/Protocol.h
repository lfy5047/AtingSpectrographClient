#pragma once

#include <cstdint>
#include <type_traits>

// Server 通讯协议 wire-format 定义（小端、与平台无关）。
// - 控制通道：TCP，CtrlHeader(16B) + JSON payload
// - 图像通道：UDP，StreamHeader(32B) + 二进制 payload
namespace srv {

// 'ATIN' 小端：0x4E495441
inline constexpr uint32_t kCtrlMagic   = 0x4E495441u;
// 'ASTM' 小端：0x4D545341
inline constexpr uint32_t kStreamMagic = 0x4D545341u;

inline constexpr uint16_t kProtoVersion = 1;
inline constexpr uint32_t kMaxCtrlPayload = 1u << 20;   // 1 MiB

enum class MsgType : uint16_t {
    Request  = 1,
    Response = 2,
    Event    = 3,
    Error    = 4,
};

// 图像通道 ID。位编号即订阅位图位号。
enum class StreamChannel : uint8_t {
    Raw16          = 1,
    SliceStitch16  = 3,
};

enum class PixelFormat : uint16_t {
    Mono8  = 1,
    Mono16 = 2,
};

enum class StreamFlag : uint16_t {
    LastFragment = 0x1,
};

#pragma pack(push, 1)
struct CtrlHeader {
    uint32_t magic;        // kCtrlMagic
    uint16_t version;      // kProtoVersion
    uint16_t type;         // MsgType
    uint32_t seq;          // request 自增；response 回填；event=0
    uint32_t payload_len;  // JSON 字节数
};

struct StreamHeader {
    uint32_t magic;        // kStreamMagic
    uint8_t  version;      // kProtoVersion
    uint8_t  channel;      // StreamChannel
    uint16_t flags;        // StreamFlag 位或
    uint64_t frame_id;     // 单调递增
    uint16_t pkt_index;
    uint16_t pkt_total;
    uint32_t frame_bytes;  // 整帧 payload 字节数
    uint16_t width;
    uint16_t height;
    uint16_t pixel_format; // PixelFormat
    uint16_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(CtrlHeader)   == 16, "CtrlHeader must be 16 bytes");
static_assert(sizeof(StreamHeader) == 32, "StreamHeader must be 32 bytes");
static_assert(std::is_trivially_copyable_v<CtrlHeader>);
static_assert(std::is_trivially_copyable_v<StreamHeader>);

inline uint32_t channelBit(StreamChannel ch) {
    return 1u << static_cast<uint8_t>(ch);
}

} // namespace srv

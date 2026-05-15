#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "Protocol.h"
#include "ThreadSafeDataQueue.h"

namespace srv {

// UDP 图像流推送：
// - 每帧由 StreamHeader + payload 分包发送（自动按 udp_payload 切片）
// - 通道订阅位图控制是否真正发送，未订阅时 push 立即丢弃
// - 目标地址：IP 由上层（TCP peer）注入，端口由客户端 stream.subscribe 指定
// - 整帧丢弃策略由客户端按 frame_id 装配实现，服务端无状态
class UdpStreamServer {
public:
    struct Frame {
        StreamChannel channel;
        uint16_t      width;
        uint16_t      height;
        PixelFormat   pixel_format;
        // 整帧二进制数据；按 channel 自然布局（如 Mono16 每像素 2B 小端）
        std::shared_ptr<const std::vector<uint8_t>> data;
    };

    struct Config {
        std::string bind_ip      = "0.0.0.0"; // 本地 UDP 出口；0.0.0.0 由 OS 选路
        int         udp_payload  = 1400;      // 单包 payload 字节数（不含 StreamHeader）
        size_t      queue_cap    = 8;         // 帧投递队列容量；满则丢最旧
    };

    UdpStreamServer(boost::asio::io_context& io, Config cfg);
    ~UdpStreamServer();

    UdpStreamServer(const UdpStreamServer&) = delete;
    UdpStreamServer& operator=(const UdpStreamServer&) = delete;

    void start();
    void stop();

    // 目标：通常 IP 来自 TCP peer，端口来自 stream.subscribe
    void setTarget(const std::string& ip, uint16_t port);
    void clearTarget();
    bool hasTarget() const;

    // 订阅控制（线程安全）
    void subscribe(StreamChannel ch);
    void unsubscribe(StreamChannel ch);
    void clearSubscriptions();
    bool isSubscribed(StreamChannel ch) const;
    uint32_t enabledMask() const { return enabled_mask_; }

    // 推一帧，线程安全；未订阅或无目标时立即丢弃，不入队
    void pushFrame(Frame f);

    uint64_t framesSent()    const { return frames_sent_; }
    uint64_t framesDropped() const { return frames_dropped_; }

private:
    void loop();
    void sendFrame(const Frame& f, uint64_t fid);

    Config                       cfg_;
    boost::asio::io_context&     io_;
    boost::asio::ip::udp::socket sock_;

    mutable std::mutex                  target_mtx_;
    boost::asio::ip::udp::endpoint      target_;
    bool                                target_set_ = false;

    std::atomic<uint32_t> enabled_mask_  {0};
    std::atomic<uint64_t> next_frame_id_ {1};
    std::atomic<uint64_t> frames_sent_   {0};
    std::atomic<uint64_t> frames_dropped_{0};

    ThreadSafeDataQueue<Frame> queue_;
    std::atomic<bool>          running_{false};
    std::thread                worker_;
};

} // namespace srv

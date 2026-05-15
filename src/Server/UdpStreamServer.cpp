#include "UdpStreamServer.h"

#include <cstring>

#include "plog/Log.h"

namespace srv {

using boost::asio::ip::udp;

UdpStreamServer::UdpStreamServer(boost::asio::io_context& io, Config cfg)
    : cfg_(std::move(cfg)), io_(io), sock_(io_) {}

UdpStreamServer::~UdpStreamServer() { stop(); }

void UdpStreamServer::start() {
    if (running_) return;
    boost::system::error_code ec;
    auto addr = boost::asio::ip::make_address(cfg_.bind_ip, ec);
    if (ec) {
        PLOGE << "udp bind_ip invalid: " << cfg_.bind_ip << " " << ec.message();
        return;
    }
    sock_.open(udp::v4(), ec);
    if (ec) { PLOGE << "udp open: " << ec.message(); return; }
    sock_.bind(udp::endpoint(addr, 0), ec);
    if (ec) { PLOGE << "udp bind: " << ec.message(); return; }

    // 大发送缓冲：单帧可能十几 MB
    boost::asio::socket_base::send_buffer_size sbuf(8 * 1024 * 1024);
    sock_.set_option(sbuf, ec);

    running_ = true;
    worker_ = std::thread([this] { loop(); });
    PLOGI << "UdpStreamServer started, bind " << cfg_.bind_ip;
}

void UdpStreamServer::stop() {
    if (!running_) return;
    running_ = false;
    queue_.notifyAll();
    if (worker_.joinable()) worker_.join();
    boost::system::error_code ec;
    sock_.close(ec);
}

void UdpStreamServer::setTarget(const std::string& ip, uint16_t port) {
    boost::system::error_code ec;
    auto addr = boost::asio::ip::make_address(ip, ec);
    if (ec) { PLOGW << "udp target ip invalid: " << ip; return; }
    std::lock_guard<std::mutex> lk(target_mtx_);
    target_ = udp::endpoint(addr, port);
    target_set_ = true;
}

void UdpStreamServer::clearTarget() {
    std::lock_guard<std::mutex> lk(target_mtx_);
    target_set_ = false;
}

bool UdpStreamServer::hasTarget() const {
    std::lock_guard<std::mutex> lk(target_mtx_);
    return target_set_;
}

void UdpStreamServer::subscribe(StreamChannel ch) {
    enabled_mask_ |= channelBit(ch);
}
void UdpStreamServer::unsubscribe(StreamChannel ch) {
    enabled_mask_ &= ~channelBit(ch);
}
void UdpStreamServer::clearSubscriptions() {
    enabled_mask_ = 0;
}
bool UdpStreamServer::isSubscribed(StreamChannel ch) const {
    return (enabled_mask_ & channelBit(ch)) != 0;
}

void UdpStreamServer::pushFrame(Frame f) {
    if (!running_) return;
    if (!(enabled_mask_ & channelBit(f.channel))) return;
    if (!hasTarget()) return;
    if (!f.data || f.data->empty()) return;
    queue_.pushDropOldest(std::move(f), cfg_.queue_cap);
}

void UdpStreamServer::loop() {
    while (running_) {
        Frame f;
        if (!queue_.waitPopFor(f, 50, running_)) continue;
        if (!hasTarget()) { ++frames_dropped_; continue; }
        if (!(enabled_mask_ & channelBit(f.channel))) { ++frames_dropped_; continue; }
        uint64_t fid = next_frame_id_++;
        sendFrame(f, fid);
        ++frames_sent_;
    }
}

void UdpStreamServer::sendFrame(const Frame& f, uint64_t fid) {
    const auto& buf = *f.data;
    const uint32_t total = static_cast<uint32_t>(buf.size());
    const int  payload_sz = std::max(64, cfg_.udp_payload);
    const uint32_t pkt_total = (total + payload_sz - 1) / payload_sz;
    if (pkt_total == 0 || pkt_total > 0xFFFF) {
        PLOGW << "udp frame size out of range: " << total;
        ++frames_dropped_;
        return;
    }

    udp::endpoint to;
    {
        std::lock_guard<std::mutex> lk(target_mtx_);
        if (!target_set_) { ++frames_dropped_; return; }
        to = target_;
    }

    std::vector<uint8_t> pkt(sizeof(StreamHeader) + payload_sz);
    StreamHeader hdr{};
    hdr.magic        = kStreamMagic;
    hdr.version      = kProtoVersion;
    hdr.channel      = static_cast<uint8_t>(f.channel);
    hdr.frame_id     = fid;
    hdr.pkt_total    = static_cast<uint16_t>(pkt_total);
    hdr.frame_bytes  = total;
    hdr.width        = f.width;
    hdr.height       = f.height;
    hdr.pixel_format = static_cast<uint16_t>(f.pixel_format);

    for (uint32_t i = 0; i < pkt_total; ++i) {
        uint32_t off = i * payload_sz;
        uint32_t len = std::min<uint32_t>(payload_sz, total - off);
        hdr.pkt_index = static_cast<uint16_t>(i);
        hdr.flags = (i + 1 == pkt_total)
            ? static_cast<uint16_t>(StreamFlag::LastFragment) : 0;
        std::memcpy(pkt.data(), &hdr, sizeof(hdr));
        std::memcpy(pkt.data() + sizeof(hdr), buf.data() + off, len);

        boost::system::error_code ec;
        sock_.send_to(boost::asio::buffer(pkt.data(), sizeof(hdr) + len), to, 0, ec);
        if (ec) {
            // 单包失败不影响后续帧；记一次日志以免刷屏
            static thread_local int log_cnt = 0;
            if (++log_cnt % 100 == 1)
                PLOGW << "udp send_to: " << ec.message();
        }
    }
}

} // namespace srv

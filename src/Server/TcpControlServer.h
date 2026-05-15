#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "json.hpp"
#include "Protocol.h"

namespace srv {

// 单客户端 TCP 控制通道：
// - 定长 CtrlHeader + JSON payload，累积缓冲处理粘包/拆包
// - 收到一条完整请求后通过 onRequest 回调上层，再调 sendResponse 回包
// - 任意线程可调用 pushEvent 推送 type=Event 的主动消息（线程安全，post 到 io_context）
class TcpControlServer {
public:
    // (seq, cmd, params) -> 完整响应 json（含 ok/data 或 ok/code/msg）。同步返回。
    using RequestHandler = std::function<nlohmann::json(uint32_t seq,
                                                        const std::string& cmd,
                                                        const nlohmann::json& params)>;
    // 连接建立/断开回调。connected 一旦为 true，peer_ip 保证有效。
    using ConnChangedHandler = std::function<void(bool connected, std::string peer_ip)>;

    struct Config {
        std::string bind_ip = "0.0.0.0";
        uint16_t    port    = 9000;
    };

    TcpControlServer(boost::asio::io_context& io, Config cfg);
    ~TcpControlServer();

    TcpControlServer(const TcpControlServer&) = delete;
    TcpControlServer& operator=(const TcpControlServer&) = delete;

    void setRequestHandler(RequestHandler h)  { onRequest_ = std::move(h); }
    void setConnChangedHandler(ConnChangedHandler h) { onConn_ = std::move(h); }

    void start();
    void stop();

    // 主动事件推送（type=Event，seq=0）。线程安全。
    void pushEvent(const std::string& evt, nlohmann::json data);

    bool isConnected() const { return connected_; }
    std::string peerIp() const;

private:
    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(TcpControlServer* owner, boost::asio::ip::tcp::socket sock);
        void start();
        void close();
        void writeFrame(MsgType type, uint32_t seq, const nlohmann::json& payload);
        std::string peerIp() const { return peer_ip_; }

    private:
        void doRead();
        void tryConsume();
        void doWrite();

        TcpControlServer*               owner_;
        boost::asio::ip::tcp::socket    sock_;
        std::string                     peer_ip_;

        std::vector<uint8_t>            rx_;            // 累积接收缓冲
        uint8_t                         read_chunk_[4096];

        std::deque<std::vector<uint8_t>> tx_;           // 待发送队列（io_context 线程内访问）
        bool                            writing_ = false;
        bool                            closed_  = false;
    };

    void doAccept();

    boost::asio::io_context&              io_;
    Config                                cfg_;
    boost::asio::ip::tcp::acceptor        acceptor_;
    std::shared_ptr<Session>              session_;     // 仅 io_context 线程内访问
    std::atomic<bool>                     connected_{false};
    mutable std::mutex                    peer_mtx_;
    std::string                           peer_ip_;

    RequestHandler     onRequest_;
    ConnChangedHandler onConn_;
};

} // namespace srv

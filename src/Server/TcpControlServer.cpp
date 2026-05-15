#include "TcpControlServer.h"

#include <cstring>

#include "plog/Log.h"

namespace srv {

using boost::asio::ip::tcp;

// ── Session ───────────────────────────────────────────────────────────────────

TcpControlServer::Session::Session(TcpControlServer* owner, tcp::socket sock)
    : owner_(owner), sock_(std::move(sock))
{
    boost::system::error_code ec;
    auto ep = sock_.remote_endpoint(ec);
    peer_ip_ = ec ? "" : ep.address().to_string();
}

void TcpControlServer::Session::start() {
    rx_.reserve(8192);
    doRead();
}

void TcpControlServer::Session::close() {
    if (closed_) return;
    closed_ = true;
    boost::system::error_code ec;
    sock_.shutdown(tcp::socket::shutdown_both, ec);
    sock_.close(ec);
}

void TcpControlServer::Session::doRead() {
    auto self = shared_from_this();
    sock_.async_read_some(
        boost::asio::buffer(read_chunk_, sizeof(read_chunk_)),
        [this, self](const boost::system::error_code& ec, std::size_t n) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted)
                    PLOGD << "tcp read: " << ec.message();
                owner_->session_.reset();
                owner_->connected_ = false;
                if (owner_->onConn_) owner_->onConn_(false, peer_ip_);
                close();
                return;
            }
            rx_.insert(rx_.end(), read_chunk_, read_chunk_ + n);
            tryConsume();
            if (!closed_) doRead();
        });
}

void TcpControlServer::Session::tryConsume() {
    while (true) {
        if (rx_.size() < sizeof(CtrlHeader)) return;

        CtrlHeader hdr;
        std::memcpy(&hdr, rx_.data(), sizeof(hdr));

        if (hdr.magic != kCtrlMagic || hdr.version != kProtoVersion) {
            PLOGW << "tcp bad header magic/version, dropping connection";
            close();
            return;
        }
        if (hdr.payload_len > kMaxCtrlPayload) {
            PLOGW << "tcp payload too large: " << hdr.payload_len;
            close();
            return;
        }
        size_t total = sizeof(CtrlHeader) + hdr.payload_len;
        if (rx_.size() < total) return;

        // 取出一条完整帧
        nlohmann::json body;
        if (hdr.payload_len > 0) {
            try {
                body = nlohmann::json::parse(
                    rx_.begin() + sizeof(CtrlHeader),
                    rx_.begin() + total);
            } catch (const std::exception& e) {
                writeFrame(MsgType::Error, hdr.seq,
                           nlohmann::json{{"ok", false},
                                          {"code", -2},
                                          {"msg", std::string("bad json: ") + e.what()}});
                rx_.erase(rx_.begin(), rx_.begin() + total);
                continue;
            }
        }
        rx_.erase(rx_.begin(), rx_.begin() + total);

        if (hdr.type != static_cast<uint16_t>(MsgType::Request)) {
            // 客户端只应发 Request；其它类型忽略
            continue;
        }

        std::string cmd = body.value("cmd", "");
        nlohmann::json params = body.value("params", nlohmann::json::object());

        nlohmann::json resp = owner_->onRequest_
            ? owner_->onRequest_(hdr.seq, cmd, params)
            : nlohmann::json{{"ok", false}, {"code", -1}, {"msg", "no handler bound"}};

        MsgType rt = resp.value("ok", false) ? MsgType::Response : MsgType::Error;
        writeFrame(rt, hdr.seq, resp);
    }
}

void TcpControlServer::Session::writeFrame(MsgType type, uint32_t seq,
                                            const nlohmann::json& payload) {
    auto body = payload.dump();
    std::vector<uint8_t> buf(sizeof(CtrlHeader) + body.size());
    CtrlHeader hdr{kCtrlMagic, kProtoVersion, static_cast<uint16_t>(type),
                   seq, static_cast<uint32_t>(body.size())};
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());

    tx_.push_back(std::move(buf));
    if (!writing_) doWrite();
}

void TcpControlServer::Session::doWrite() {
    if (tx_.empty()) { writing_ = false; return; }
    writing_ = true;
    auto self = shared_from_this();
    boost::asio::async_write(
        sock_,
        boost::asio::buffer(tx_.front()),
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                PLOGD << "tcp write: " << ec.message();
                writing_ = false;
                close();
                return;
            }
            tx_.pop_front();
            doWrite();
        });
}

// ── TcpControlServer ──────────────────────────────────────────────────────────

TcpControlServer::TcpControlServer(boost::asio::io_context& io, Config cfg)
    : io_(io), cfg_(std::move(cfg)), acceptor_(io_) {}

TcpControlServer::~TcpControlServer() { stop(); }

void TcpControlServer::start() {
    boost::system::error_code ec;
    auto addr = boost::asio::ip::make_address(cfg_.bind_ip, ec);
    if (ec) {
        PLOGE << "tcp bind_ip invalid: " << cfg_.bind_ip << " " << ec.message();
        return;
    }
    tcp::endpoint ep(addr, cfg_.port);

    acceptor_.open(ep.protocol(), ec);
    if (ec) { PLOGE << "tcp open: " << ec.message(); return; }
    acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(ep, ec);
    if (ec) { PLOGE << "tcp bind: " << ec.message(); return; }
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { PLOGE << "tcp listen: " << ec.message(); return; }

    PLOGI << "TcpControlServer listening on " << cfg_.bind_ip << ":" << cfg_.port;
    doAccept();
}

void TcpControlServer::stop() {
    boost::system::error_code ec;
    acceptor_.close(ec);
    boost::asio::post(io_, [this] {
        if (session_) { session_->close(); session_.reset(); }
        connected_ = false;
    });
}

void TcpControlServer::doAccept() {
    acceptor_.async_accept(
        [this](const boost::system::error_code& ec, tcp::socket sock) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted)
                    PLOGD << "tcp accept: " << ec.message();
                return;
            }
            // 单客户端：已有连接则关闭新到来的
            if (session_) {
                PLOGW << "tcp: reject new connection (single-client mode)";
                boost::system::error_code _ec;
                sock.close(_ec);
            } else {
                session_ = std::make_shared<Session>(this, std::move(sock));
                {
                    std::lock_guard<std::mutex> lk(peer_mtx_);
                    peer_ip_ = session_->peerIp();
                }
                connected_ = true;
                PLOGI << "tcp: client connected " << peer_ip_;
                if (onConn_) onConn_(true, peer_ip_);
                session_->start();
            }
            doAccept();
        });
}

void TcpControlServer::pushEvent(const std::string& evt, nlohmann::json data) {
    auto payload = nlohmann::json{{"evt", evt}, {"data", std::move(data)}};
    boost::asio::post(io_, [this, payload = std::move(payload)] {
        if (session_) session_->writeFrame(MsgType::Event, 0, payload);
    });
}

std::string TcpControlServer::peerIp() const {
    std::lock_guard<std::mutex> lk(peer_mtx_);
    return peer_ip_;
}

} // namespace srv

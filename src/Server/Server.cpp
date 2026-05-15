#include "Server.h"

#include <algorithm>
#include <cstring>

#include <opencv2/imgproc.hpp>

#include "plog/Log.h"

#include "CameraCore.h"
#include "CommonDefine.h"
#include "GigEVisionClient.h"
#include "MotionCollectGate.h"
#include "RotatingMirror.h"
#include "Utils.h"

namespace srv {

namespace {

// 字符串 <-> 通道 ID
StreamChannel parseChannel(const std::string& s) {
    if (s == "raw16")           return StreamChannel::Raw16;
    if (s == "slice_stitch16")  return StreamChannel::SliceStitch16;
    throw RpcError(-3, "unknown channel: " + s);
}
const char* channelName(StreamChannel c) {
    switch (c) {
    case StreamChannel::Raw16:          return "raw16";
    case StreamChannel::SliceStitch16:  return "slice_stitch16";
    }
    return "?";
}

void requireBound(const void* p, const char* what) {
    if (!p) throw RpcError(-10, std::string(what) + " not bound");
}

} // namespace

// ── ctor / start / stop ───────────────────────────────────────────────────────

Server::Server(Config cfg)
    : cfg_(std::move(cfg)),
      tcp_(io_, TcpControlServer::Config{cfg_.control_bind_ip, cfg_.control_port}),
      udp_(io_, UdpStreamServer::Config{cfg_.stream_bind_ip,
                                        cfg_.stream_udp_payload,
                                        cfg_.stream_queue_cap})
{
    tcp_.setRequestHandler([this](uint32_t /*seq*/, const std::string& cmd,
                                  const nlohmann::json& params) {
        return router_.dispatch(cmd, params);
    });
    tcp_.setConnChangedHandler([this](bool c, std::string ip) { onConnChanged(c, std::move(ip)); });

    registerHandlers();
}

Server::~Server() { stop(); }

void Server::start() {
    if (running_) return;
    running_ = true;
    work_ = std::make_unique<WorkGuard>(boost::asio::make_work_guard(io_));
    io_thread_ = std::thread([this] {
        try { io_.run(); }
        catch (const std::exception& e) { PLOGE << "io_context: " << e.what(); }
    });
    tcp_.start();
    udp_.start();
    PLOGI << "Server started";
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    udp_.stop();
    tcp_.stop();
    work_.reset();
    io_.stop();
    if (io_thread_.joinable()) io_thread_.join();
    io_.restart();
    PLOGI << "Server stopped";
}

// ── bind helpers ──────────────────────────────────────────────────────────────

void Server::bindMirror(RotatingMirror* m) {
    mirror_ = m;
    if (!mirror_) return;
    // mirror.angle 事件主推
    mirror_->sig_queryAngle.connect(mirror_,
        [this](bool ok, double angle, bool is_moving, int64_t /*ts*/) {
            if (!ok) return;
            tcp_.pushEvent("mirror.angle",
                nlohmann::json{{"angle", angle},
                               {"is_moving", is_moving},
                               {"ts", Utils::getCurrentTimestampNs()}});
        }, ss::ConnType::Direct);
}

void Server::bindCamera(GigEVisionClient* c)    { camera_    = c; }
void Server::bindIr(CameraCore* ir)             { ir_        = ir; }
void Server::bindProcessor(FrameProcessor* p)   { processor_ = p; }
void Server::bindCollect(MotionCollectGate* g)  { collect_   = g; }

// ── connection lifecycle ──────────────────────────────────────────────────────

void Server::onConnChanged(bool connected, std::string peer_ip) {
    if (connected) {
        // UDP 目标 IP 复用 TCP peer；端口需 stream.subscribe 时携带
        udp_.clearTarget();
        // 临时仅记录 IP，等 subscribe 才真正 setTarget(port)。
        // 实现简化：在 subscribe 时若未注入 IP 也用当前 peer。
        PLOGI << "Server: client up " << peer_ip;
    } else {
        udp_.clearSubscriptions();
        udp_.clearTarget();
        PLOGI << "Server: client down";
    }
}

// ── 图像入口 ──────────────────────────────────────────────────────────────────

void Server::pushRaw(const RawFrame& f) {
    const uint32_t mask = udp_.enabledMask();
    if (!(mask & channelBit(StreamChannel::Raw16))) return;

    auto bytes = std::make_shared<std::vector<uint8_t>>(f.data.size() * sizeof(uint16_t));
    std::memcpy(bytes->data(), f.data.data(), bytes->size());
    udp_.pushFrame({StreamChannel::Raw16,
                    static_cast<uint16_t>(f.width),
                    static_cast<uint16_t>(f.height),
                    PixelFormat::Mono16,
                    bytes});
}

void Server::pushSliceStitch(const cv::Mat& img16) {
    if (img16.empty() || img16.type() != CV_16UC1) return;
    if (!(udp_.enabledMask() & channelBit(StreamChannel::SliceStitch16))) return;
    udp_.pushFrame({StreamChannel::SliceStitch16,
                    static_cast<uint16_t>(img16.cols),
                    static_cast<uint16_t>(img16.rows),
                    PixelFormat::Mono16,
                    mat16ToBytes(img16)});
}

// ── 图像格式转换 ──────────────────────────────────────────────────────────────

std::shared_ptr<std::vector<uint8_t>> Server::mat16ToBytes(const cv::Mat& m) {
    const size_t bytes = static_cast<size_t>(m.cols) * m.rows * sizeof(uint16_t);
    auto out = std::make_shared<std::vector<uint8_t>>(bytes);
    if (m.isContinuous()) {
        std::memcpy(out->data(), m.ptr<uint16_t>(), bytes);
    } else {
        size_t row_bytes = static_cast<size_t>(m.cols) * sizeof(uint16_t);
        for (int r = 0; r < m.rows; ++r)
            std::memcpy(out->data() + r * row_bytes, m.ptr<uint16_t>(r), row_bytes);
    }
    return out;
}

// ── 命令路由表 ────────────────────────────────────────────────────────────────

void Server::registerHandlers() {
    using J = nlohmann::json;

    // system.*
    router_.registerCmd("system.ping", [](const J&) {
        return J{{"ts", Utils::getCurrentTimestampNs()}};
    });
    router_.registerCmd("system.version", [](const J&) {
        return J{{"version", kProtoVersion}, {"name", "AtingSpectrograph"}};
    });
    router_.registerCmd("system.status", [this](const J&) {
        return J{{"tcp_connected", tcp_.isConnected()},
                 {"udp_target",    udp_.hasTarget()},
                 {"channels_mask", udp_.enabledMask()},
                 {"frames_sent",   udp_.framesSent()},
                 {"frames_dropped",udp_.framesDropped()}};
    });

    // mirror.*
    router_.registerCmd("mirror.query_angle", [this](const J&) {
        requireBound(mirror_, "mirror");
        double angle = 0; bool moving = false;
        bool ok = mirror_->queryAngleSync(angle, moving);
        if (!ok) throw RpcError(-20, "mirror query timeout");
        return J{{"angle", angle}, {"is_moving", moving}};
    });
    router_.registerCmd("mirror.set_speed", [this](const J& p) {
        requireBound(mirror_, "mirror");
        int s = p.at("s_speed").get<int>();
        int f = p.at("f_speed").get<int>();
        if (!mirror_->setSpeedSync(s, f)) throw RpcError(-20, "mirror set_speed timeout");
        return J::object();
    });
    router_.registerCmd("mirror.set_target", [this](const J& p) {
        requireBound(mirror_, "mirror");
        double deg = p.at("angle").get<double>();
        if (!mirror_->setTargetSync(deg)) throw RpcError(-20, "mirror set_target timeout");
        return J::object();
    });
    router_.registerCmd("mirror.set_target_absolute", [this](const J& p) {
        requireBound(mirror_, "mirror");
        double deg = p.at("angle").get<double>();
        if (!mirror_->setTargetAbsoluteSync(deg)) throw RpcError(-20, "mirror set_target_absolute timeout");
        return J::object();
    });
    router_.registerCmd("mirror.start_move", [this](const J&) {
        requireBound(mirror_, "mirror");
        if (!mirror_->startMoveSync()) throw RpcError(-20, "mirror start_move timeout");
        return J::object();
    });
    router_.registerCmd("mirror.stop_move", [this](const J&) {
        requireBound(mirror_, "mirror");
        if (!mirror_->stopMoveSync()) throw RpcError(-20, "mirror stop_move timeout");
        return J::object();
    });
    router_.registerCmd("mirror.home", [this](const J&) {
        requireBound(mirror_, "mirror");
        if (!mirror_->homeSync()) throw RpcError(-20, "mirror home timeout");
        return J::object();
    });
    router_.registerCmd("mirror.set_home", [this](const J&) {
        requireBound(mirror_, "mirror");
        if (!mirror_->setCurrentPointAsHomeSync()) throw RpcError(-20, "mirror set_home timeout");
        return J::object();
    });
    router_.registerCmd("mirror.goto_preset", [this](const J& p) {
        requireBound(mirror_, "mirror");
        int id = p.at("id").get<int>();
        if (!mirror_->gotoPresetAngleSync(id)) throw RpcError(-20, "mirror goto_preset timeout");
        return J::object();
    });

    // ir.*  红外机芯（通用透传 + 几个常用命名命令）
    auto irCall = [this](uint8_t cmd, const std::vector<uint8_t>& data,
                          uint8_t readback_len) {
        requireBound(ir_, "ir");
        auto r = ir_->send_sync(static_cast<CameraCore::Command>(cmd), data, readback_len);
        if (!r.ok) throw RpcError(-21, r.error.empty() ? "ir transact failed" : r.error);
        return nlohmann::json{{"cmd", r.cmd}, {"data", r.data}};
    };
    router_.registerCmd("ir.send_raw", [irCall](const J& p) {
        uint8_t cmd = p.at("cmd").get<uint8_t>();
        std::vector<uint8_t> data = p.value("data", std::vector<uint8_t>{});
        uint8_t len = p.value("readback_len", uint8_t{1});
        return irCall(cmd, data, len);
    });
    router_.registerCmd("ir.trigger_calibration", [irCall](const J&) {
        return irCall(0x02, {0x01}, 1);
    });
    router_.registerCmd("ir.set_brightness", [irCall](const J& p) {
        uint8_t v = p.at("value").get<uint8_t>();
        return irCall(0x08, {v}, 1);
    });
    router_.registerCmd("ir.set_contrast", [irCall](const J& p) {
        uint8_t v = p.at("value").get<uint8_t>();
        return irCall(0x0A, {v}, 1);
    });
    router_.registerCmd("ir.set_integration", [irCall](const J& p) {
        uint16_t v = p.at("value").get<uint16_t>();
        return irCall(0x1D, {static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v & 0xFF)}, 1);
    });
    router_.registerCmd("ir.read_module_id",        [irCall](const J&){ return irCall(0x20, {}, 8); });
    router_.registerCmd("ir.read_self_check",        [irCall](const J&){ return irCall(0x21, {}, 1); });
    router_.registerCmd("ir.read_focus_plane_temp",  [irCall](const J&){ return irCall(0x22, {}, 2); });
    router_.registerCmd("ir.read_core_temp",         [irCall](const J&){ return irCall(0x25, {}, 2); });
    router_.registerCmd("ir.query_integration_time", [irCall](const J&){ return irCall(0x28, {}, 2); });

    // camera.*  GigE 相机控制
    router_.registerCmd("camera.start_stream", [this](const J&) {
        requireBound(camera_, "camera");
        camera_->startStream();
        return J::object();
    });
    router_.registerCmd("camera.stop_stream", [this](const J&) {
        requireBound(camera_, "camera");
        camera_->stopStream();
        return J::object();
    });
    router_.registerCmd("camera.get_resolution", [this](const J&) {
        requireBound(camera_, "camera");
        return J{{"width", camera_->width}, {"height", camera_->height}};
    });
    router_.registerCmd("camera.set_resolution", [this](const J& p) {
        requireBound(camera_, "camera");
        camera_->changeResolution(p.at("width").get<int>(), p.at("height").get<int>());
        return J::object();
    });

    // collect.*  运动采集门控
    router_.registerCmd("collect.start", [this](const J&) {
        requireBound(collect_, "collect");
        collect_->startCollect();
        return J::object();
    });
    router_.registerCmd("collect.stop", [this](const J&) {
        requireBound(collect_, "collect");
        collect_->stopCollect();
        return J::object();
    });
    router_.registerCmd("collect.status", [this](const J&) {
        requireBound(collect_, "collect");
        return J{{"is_collecting", collect_->isCollecting()}};
    });

    // stream.*  UDP 通道订阅
    router_.registerCmd("stream.subscribe", [this](const J& p) {
        if (!tcp_.isConnected()) throw RpcError(-30, "no tcp connection");
        uint16_t port = p.at("port").get<uint16_t>();
        udp_.setTarget(tcp_.peerIp(), port);
        auto chs = p.at("channels");
        std::vector<std::string> applied;
        for (auto& c : chs) {
            auto ch = parseChannel(c.get<std::string>());
            udp_.subscribe(ch);
            applied.emplace_back(channelName(ch));
        }
        return J{{"subscribed", applied},
                 {"target_ip", tcp_.peerIp()},
                 {"target_port", port}};
    });
    router_.registerCmd("stream.unsubscribe", [this](const J& p) {
        if (p.value("all", false)) {
            udp_.clearSubscriptions();
            return J{{"all", true}};
        }
        std::vector<std::string> applied;
        for (auto& c : p.at("channels")) {
            auto ch = parseChannel(c.get<std::string>());
            udp_.unsubscribe(ch);
            applied.emplace_back(channelName(ch));
        }
        return J{{"unsubscribed", applied}};
    });
    router_.registerCmd("stream.status", [this](const J&) {
        std::vector<std::string> active;
        for (auto ch : {StreamChannel::Raw16,
                        StreamChannel::SliceStitch16})
            if (udp_.isSubscribed(ch)) active.emplace_back(channelName(ch));
        return J{{"channels", active},
                 {"frames_sent", udp_.framesSent()},
                 {"frames_dropped", udp_.framesDropped()},
                 {"has_target", udp_.hasTarget()}};
    });
}

} // namespace srv

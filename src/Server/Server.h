#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio.hpp>
#include <opencv2/core.hpp>

#include "CommandRouter.h"
#include "TcpControlServer.h"
#include "UdpStreamServer.h"

// 前向声明，避免在头文件污染依赖图
class RotatingMirror;
class GigEVisionClient;
class CameraCore;
class FrameProcessor;
class MotionCollectGate;
struct RawFrame;

namespace srv {

// 服务端通讯顶层 facade：
// - 持有 TCP 控制 / UDP 图像流两个 server 与共享 io_context
// - 通过 bindXxx 注入设备/算法/采集模块，向 router 注册命令
// - 数据源由上层（如 App）在产帧信号槽中调用 pushXxx 转发到 UDP 通道
class Server {
public:
    struct Config {
        std::string control_bind_ip   = "0.0.0.0";
        uint16_t    control_port      = 9000;
        std::string stream_bind_ip    = "0.0.0.0";
        int         stream_udp_payload = 1400;
        size_t      stream_queue_cap  = 8;
    };

    explicit Server(Config cfg);
    ~Server();

    void start();
    void stop();

    // 设备注入；可任意顺序在 start() 前后调用，但建议 start() 前调好
    void bindMirror(RotatingMirror* m);
    void bindCamera(GigEVisionClient* c);
    void bindIr(CameraCore* ir);
    void bindProcessor(FrameProcessor* p);
    void bindCollect(MotionCollectGate* g);

    // 图像源 -> UDP 通道。按当前订阅位图与 PixelFormat 自动选择是否真正发送。
    // Raw16 来自原始帧。
    void pushRaw(const RawFrame& frame);
    void pushSliceStitch(const cv::Mat& img16);

private:
    void registerHandlers();
    void onConnChanged(bool connected, std::string peer_ip);

    // 把 16bit Mat 转为 vector<uint8_t>（Mono16 raw bytes）；非连续时按行拷贝
    static std::shared_ptr<std::vector<uint8_t>> mat16ToBytes(const cv::Mat& m);

    Config cfg_;
    boost::asio::io_context io_;
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    std::unique_ptr<WorkGuard> work_;
    std::thread io_thread_;

    TcpControlServer tcp_;
    UdpStreamServer  udp_;
    CommandRouter    router_;
    std::atomic<bool> running_{false};

    RotatingMirror*    mirror_    = nullptr;
    GigEVisionClient*  camera_    = nullptr;
    CameraCore*        ir_        = nullptr;
    FrameProcessor*    processor_ = nullptr;
    MotionCollectGate* collect_   = nullptr;
};

} // namespace srv

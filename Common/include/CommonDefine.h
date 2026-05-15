#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

#include <string>
#include <vector>

// 转镜状态数据
struct RotatingMirrorFrame {
    int64_t timestamp; // 时间戳 单位: 纳秒
    double angle; // 角度
    RotatingMirrorFrame(int64_t timestamp, double angle) : timestamp(timestamp), angle(angle) {}
    RotatingMirrorFrame() : timestamp(0), angle(0.0) {}
};

// 数据帧类型
enum class FrameType {
    // 头帧
    HeaderFrame,
    // 数据帧
    DataFrame,
    // 尾帧
    TailFrame,
};

// 原始帧数据
struct RawFrame {
    std::vector<uint16_t> data;
    int width;
    int height;
    int64_t timestamp; // 时间戳 单位: 纳秒

    RotatingMirrorFrame rotating_mirror_frame;
    bool is_latest_mirror_frame = false; // 是否是最新的转镜帧

    FrameType frame_type;

    RawFrame() : data(std::vector<uint16_t>()), width(0), height(0), timestamp(0), frame_type(FrameType::DataFrame) {}
    RawFrame(const std::vector<uint16_t>& data, int width, int height) : data(data), width(width), height(height), timestamp(0), frame_type(FrameType::DataFrame) {}
};


// 切片拼接帧数据
struct SliceStitchFrame {
    std::vector<uint16_t> data;
    int X; // 宽度
    int Y; // 高度
    int C; // 通道数
    int bytesPerPixel; // 每个像素的字节数
    int64_t timestamp; // 时间戳 单位: 纳秒
};

// 区域拼接帧数据
struct RegionStitchFrame {
    std::vector<std::vector<uint16_t>> data;
    int width; // 宽度
    int height; // 高度
    int count; // 区域数量
    int bytesPerPixel; // 每个像素的字节数
    int64_t timestamp; // 时间戳 单位: 纳秒
};

// 保存触发模式
enum class SaveTrigger {
    ByFrameCount,  // 每N帧保存一个文件
    ByDuration,    // 每N毫秒保存一个文件
};

// 切片拼接数据保存配置
struct SliceStitchDataSaveConfig {
    std::string save_dir;    // 保存目录
    SaveTrigger trigger;    // 保存触发模式
    int frames_per_file;      // ByFrameCount: 每文件帧数
    int ms_per_file;          // ByDuration:   每文件时长
    uint64_t max_disk_bytes;  // 磁盘用量上限
    size_t queue_capacity;   // 内部队列容量，超出丢弃最旧帧

    SliceStitchDataSaveConfig(){
        save_dir = "./raw_data/slice_stitch_data";
        trigger = SaveTrigger::ByFrameCount;
        frames_per_file = 100;
        ms_per_file = 1000;
        max_disk_bytes = 10ULL << 30;
        queue_capacity = 512;
    }
};

// 区域拼接数据保存配置
struct RegionStitchDataSaveConfig {
    std::string save_dir;    // 保存目录
    SaveTrigger trigger;    // 保存触发模式
    int frames_per_file;      // ByFrameCount: 每文件帧数
    int ms_per_file;          // ByDuration:   每文件时长
    uint64_t max_disk_bytes;  // 磁盘用量上限
    size_t queue_capacity;   // 内部队列容量，超出丢弃最旧帧

    RegionStitchDataSaveConfig(){
        save_dir = "./raw_data/region_stitch_data";
        trigger = SaveTrigger::ByFrameCount;
        frames_per_file = 100;
        ms_per_file = 1000;
        max_disk_bytes = 10ULL << 30;
        queue_capacity = 512;
    }
};

// 原始数据保存配置
struct RawDataSaveConfig {
    std::string save_dir;    // 保存目录
    uint64_t max_disk_bytes;  // 磁盘用量上限
    size_t queue_capacity;   // 内部队列容量，超出丢弃最旧帧

    RawDataSaveConfig(){
        save_dir = "./raw_data/raw_data";
        max_disk_bytes = 10ULL << 30;
        queue_capacity = 512;
    }
};

// 串口参数
struct SerialPortConf{

    enum class BaudRate {
        BaudRate1200, // 1200 波特率
        BaudRate2400, // 2400 波特率
        BaudRate4800, // 4800 波特率
        BaudRate9600, // 9600 波特率
        BaudRate19200, // 19200 波特率
        BaudRate38400, // 38400 波特率
        BaudRate57600, // 57600 波特率  
        BaudRate115200, // 115200 波特率
        BaudRate230400, // 230400 波特率
        BaudRate460800, // 460800 波特率
        BaudRate500000, // 500000 波特率
        BaudRate576000, // 576000 波特率
        BaudRate921600, // 921600 波特率
        BaudRate1000000, // 1000000 波特率
        BaudRate1152000, // 1152000 波特率
        BaudRate1500000, // 1500000 波特率
        BaudRate2000000, // 2000000 波特率
        BaudRate2500000, // 2500000 波特率
        BaudRate3000000, // 3000000 波特率
        BaudRate3500000, // 3500000 波特率
        BaudRate4000000, // 4000000 波特率
    };
    /**
        * @brief 数据位
        */
    enum class DataBits {
        DataBits5, // 5 数据位
        DataBits6, // 6 数据位
        DataBits7, // 7 数据位  
        DataBits8, // 8 数据位
    };

    /**
        * @brief 停止位
        */
    enum class StopBits {
        StopBits1, // 1 停止位
        StopBits2, // 2 停止位
    };

    /**
        * @brief 校验位
        */
    enum class Parity {
        ParityNone, // 无校验
        ParityEven, // 偶校验
        ParityOdd, // 奇校验
    };

    std::string port; // 串口
    BaudRate baud; // 波特率
    DataBits data_bits; // 数据位
    StopBits stop_bits; // 停止位
    Parity parity; // 校验位


    SerialPortConf()
    {
        port = "";
        baud = BaudRate::BaudRate9600;
        data_bits = DataBits::DataBits8;
        stop_bits = StopBits::StopBits1;
        parity = Parity::ParityNone;
    }

    SerialPortConf(const std::string& port)
    {
        this->port = port;
        baud = BaudRate::BaudRate9600;
        data_bits = DataBits::DataBits8;
        stop_bits = StopBits::StopBits1;
        parity = Parity::ParityNone;
    }

    static BaudRate getBaudRate(const int baud_rate) {
        switch(baud_rate) {
            case 1200: return BaudRate::BaudRate1200;
            case 2400: return BaudRate::BaudRate2400;
            case 4800: return BaudRate::BaudRate4800;
            case 9600: return BaudRate::BaudRate9600;
            case 19200: return BaudRate::BaudRate19200;
            case 38400: return BaudRate::BaudRate38400;
            case 57600: return BaudRate::BaudRate57600;
            case 115200: return BaudRate::BaudRate115200;
            case 230400: return BaudRate::BaudRate230400;
            case 460800: return BaudRate::BaudRate460800;
            case 500000: return BaudRate::BaudRate500000;
            case 576000: return BaudRate::BaudRate576000;
            case 921600: return BaudRate::BaudRate921600;
            case 1000000: return BaudRate::BaudRate1000000;
            case 1152000: return BaudRate::BaudRate1152000;
            case 1500000: return BaudRate::BaudRate1500000;
            case 2000000: return BaudRate::BaudRate2000000;
            case 2500000: return BaudRate::BaudRate2500000;
            case 3000000: return BaudRate::BaudRate3000000;
            case 3500000: return BaudRate::BaudRate3500000;
            case 4000000: return BaudRate::BaudRate4000000;
            default: return BaudRate::BaudRate9600;
        }
    }

    static DataBits getDataBits(const int data_bits) {
        switch(data_bits) {
            case 5: return DataBits::DataBits5;
            case 6: return DataBits::DataBits6;
            case 7: return DataBits::DataBits7;
            case 8: return DataBits::DataBits8;
            default: return DataBits::DataBits8;
        }
    }

    static StopBits getStopBits(const int stop_bits) {
        switch(stop_bits) {
            case 1: return StopBits::StopBits1;
            case 2: return StopBits::StopBits2;
            default: return StopBits::StopBits1;
        }
    }

    static Parity getParity(const int parity) {
        switch(parity) {
            case 0: return Parity::ParityNone;
            case 1: return Parity::ParityOdd;
            case 2: return Parity::ParityEven;
            default: return Parity::ParityNone;
        }
    }
};

// 转镜预设角度
struct RotatingMirrorPresetAngle{
    int id;
    std::string name;
    double angle;
    int s_speed;
    int f_speed;
    RotatingMirrorPresetAngle()
    {
        id = 0;
        name = "point_0";
        angle = 0.0;
        s_speed = 400;
        f_speed = 400;
    }
};

// 转镜配置参数
struct RotatingMirrorConf{
    SerialPortConf serial_port_conf; // 串口配置
    // 当前角度
    double cur_angle;
    // 转动速度
    int s_speed;
    int f_speed;

    // 预设角度
    std::vector<RotatingMirrorPresetAngle> preset_angles;

    RotatingMirrorConf()
    {
        serial_port_conf = SerialPortConf();
        cur_angle = 0.0;
        s_speed = 400;
        f_speed = 400;
    }
};



struct CameraConf{
    std::string local_ip;
    int width;
    int height;
    int fps;
    SerialPortConf control_serial;
    CameraConf()
    {
        local_ip = "169.254.197.111";
        width = 1280;
        height = 1024;
        fps = 30;
        control_serial = SerialPortConf();
    }
};

// 通讯服务端配置
struct ServerConf{
    std::string control_bind_ip;
    int control_port;
    std::string stream_bind_ip;
    int stream_udp_payload;
    int stream_queue_cap;

    ServerConf()
    {
        control_bind_ip = "0.0.0.0";
        control_port = 9000;
        stream_bind_ip = "0.0.0.0";
        stream_udp_payload = 1400;
        stream_queue_cap = 8;
    }
};

struct SliceStitchAlgoConf{
    int raw_width;
    int raw_height;
    int slice_begin;
    int slice_end;
    int slice_h_begin;
    int slice_h_end;
    int stitch_count;
    SliceStitchAlgoConf()
    {
        raw_width = 640;
        raw_height = 512;
        slice_begin = 165;
        slice_end = 350;
        slice_h_begin = 50;
        slice_h_end = 462;
        stitch_count = 800;
    }
};

#endif // COMMON_DEFINE_H

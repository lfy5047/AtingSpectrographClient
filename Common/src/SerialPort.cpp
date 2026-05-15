#include "SerialPort.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include "plog/Log.h"

SerialPort::SerialPort() : fd_(-1), is_open_(false) {
}

SerialPort::~SerialPort() {
    close();
}

SerialPort::SerialPort(SerialPort&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool SerialPort::open(const SerialPortConf& conf) {
    return open(conf.port, conf.baud, conf.data_bits, conf.stop_bits, conf.parity);
}

bool SerialPort::open(const std::string& port_name, 
    BaudRate baud_rate, DataBits data_bits, 
    StopBits stop_bits, Parity parity) 
{
    // O_RDWR: 读写模式
    // O_NOCTTY: 该串口不作为控制终端
    // O_NDELAY: 非阻塞模式打开，但随后在 configure 中会重置为阻塞带超时模式
    fd_ = ::open(port_name.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    
    if (fd_ < 0) {
        PLOGE << "无法打开串口: " << port_name;
        return false;
    }
    
    // 恢复为阻塞模式
    fcntl(fd_, F_SETFL, 0);

    is_open_ = configure(baud_rate, data_bits, stop_bits, parity);
    return is_open_;
}

bool SerialPort::close() {
    if (is_open_) {
        ::close(fd_);
        fd_ = -1;
        is_open_ = false;
    }
    return is_open_;
}

bool SerialPort::configure(BaudRate baud_rate,
    DataBits data_bits,
    StopBits stop_bits,
    Parity parity) 
{
    // 获取当前配置
    struct termios options;
    if (tcgetattr(fd_, &options) < 0) {
        LOGE << "获取串口配置失败";
        close();
        return false;
    }

    // 设置波特率
    speed_t baud = get_baud_rate_enum(baud_rate);

    
    if (cfsetospeed(&options, baud) < 0 || cfsetispeed(&options, baud) < 0) {
        LOGE << "设置波特率失败";
        close();
        return false;
    }

    // 设置数据位、校验位和停止位
    options.c_cflag &= ~CSIZE; // 清除数据位设置
    switch(data_bits) {
        case DataBits::DataBits5: options.c_cflag |= CS5; break;
        case DataBits::DataBits6: options.c_cflag |= CS6; break;
        case DataBits::DataBits7: options.c_cflag |= CS7; break;
        case DataBits::DataBits8: options.c_cflag |= CS8; break;
        default:
            LOGE << "不支持的数据位: " << (int)data_bits << ", 使用默认8位";
            options.c_cflag |= CS8;
            break;
    }

    switch(parity) {
        case Parity::ParityNone: options.c_cflag &= ~PARENB; break;
        case Parity::ParityOdd: options.c_cflag |= (PARENB | PARODD); break;
        case Parity::ParityEven: options.c_cflag |= PARENB; options.c_cflag &= ~PARODD; break;
        default:
            LOGE << "不支持的校验位: " << (int)parity;
            options.c_cflag &= ~PARENB;
            break;
    }

    switch(stop_bits) {
        case StopBits::StopBits1: options.c_cflag &= ~CSTOPB; break;
        case StopBits::StopBits2: options.c_cflag |= CSTOPB; break;
        default:
            LOGE << "不支持的停止位: " << (int)stop_bits;
            options.c_cflag &= ~CSTOPB;
            break;
    }
    // 设置为原始模式 (raw mode)
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); 
    options.c_oflag &= ~OPOST;
    options.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | ISTRIP); // 禁用软件流控和CR/LF转换

    // 设置读取超时
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;

    // 应用设置
    if (tcsetattr(fd_, TCSANOW, &options) < 0) {
        LOGE << "应用串口设置失败";
        close();
        return false;
    }
    is_open_ = true;
    return true;
}

speed_t SerialPort::get_baud_rate_enum(BaudRate baud_rate) {
    switch (baud_rate) {
        case BaudRate::BaudRate1200: return B1200;
        case BaudRate::BaudRate2400: return B2400;
        case BaudRate::BaudRate4800: return B4800;
        case BaudRate::BaudRate9600: return B9600;
        case BaudRate::BaudRate19200: return B19200;
        case BaudRate::BaudRate38400: return B38400;
        case BaudRate::BaudRate57600: return B57600;
        case BaudRate::BaudRate115200: return B115200;
        case BaudRate::BaudRate230400: return B230400;
        case BaudRate::BaudRate460800: return B460800;
        case BaudRate::BaudRate500000: return B500000;
        case BaudRate::BaudRate576000: return B576000;
        case BaudRate::BaudRate921600: return B921600;
        case BaudRate::BaudRate1000000: return B1000000;
        case BaudRate::BaudRate1152000: return B1152000;
        case BaudRate::BaudRate1500000: return B1500000;
        case BaudRate::BaudRate2000000: return B2000000;
        case BaudRate::BaudRate2500000: return B2500000;
        case BaudRate::BaudRate3000000: return B3000000;
        case BaudRate::BaudRate3500000: return B3500000;
        case BaudRate::BaudRate4000000: return B4000000;
        default:
            PLOGE << "不支持的波特率: " << (int)baud_rate;
            return B9600;
    }
}

size_t SerialPort::write(const std::vector<uint8_t>& data) {
    if (!is_open_){
        LOGW << "串口未打开";
        return 0;
    }
    if (data.empty()){
        LOGW << "数据为空";
        return 0;
    }
    ssize_t written = ::write(fd_, data.data(), data.size());
    if (written < 0) {
        PLOGE << "串口写入失败" ;
        return 0;
    }
    return static_cast<size_t>(written);
}

size_t SerialPort::write(const std::string& data) {
    if (!is_open_){
        LOGW << "串口未打开";
        return 0;
    }
    if (data.empty()){
        LOGW << "数据为空";
        return 0;
    }
    ssize_t written = ::write(fd_, data.c_str(), data.length());
    if (written < 0) {
        PLOGE << "串口写入失败" ;
        return 0;
    }
    return static_cast<size_t>(written);
}

std::vector<uint8_t> SerialPort::read(size_t max_bytes) {
    if (!is_open_){
        LOGW << "串口未打开";
        return std::vector<uint8_t>();
    }
    if (max_bytes <= 0){
        LOGW << "max_bytes小于等于0";
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> buffer(max_bytes);
    ssize_t bytes_read = ::read(fd_, buffer.data(), buffer.size());
    
    if (bytes_read < 0) {
        PLOGE << "串口读取失败" ;
        return std::vector<uint8_t>();
    }
    
    buffer.resize(bytes_read); // 裁剪到实际读取的大小
    return buffer;
}

void SerialPort::flush() {
    tcflush(fd_, TCIOFLUSH);
}
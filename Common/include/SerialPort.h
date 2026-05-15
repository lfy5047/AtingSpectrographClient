#ifndef SERIAL_PORT_HPP
#define SERIAL_PORT_HPP

#include <string>
#include <vector>
#include <termios.h>

#include "CommonDefine.h"

using BaudRate = SerialPortConf::BaudRate;
using DataBits = SerialPortConf::DataBits;
using StopBits = SerialPortConf::StopBits;
using Parity = SerialPortConf::Parity;

class SerialPort {
public:



    // 构造函数：默认开启 8N1 (8数据位, 无校验, 1停止位)
    SerialPort();
    
    // 析构函数：自动关闭串口
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    bool open(const SerialPortConf& conf);
    bool open(const std::string& port_name, 
        BaudRate baud_rate = BaudRate::BaudRate9600,
        DataBits data_bits = DataBits::DataBits8,
        StopBits stop_bits = StopBits::StopBits1,
        Parity parity = Parity::ParityNone);
    bool close();
    bool isOpen() const { return is_open_; }

    // 发送数据 (二进制流)
    size_t write(const std::vector<uint8_t>& data);
    
    // 发送数据 (字符串)
    size_t write(const std::string& data);

    // 接收数据
    // max_bytes: 一次最多读取的字节数
    std::vector<uint8_t> read(size_t max_bytes = 1024);

    // 刷新缓冲区
    void flush();

private:
    int fd_; // 文件描述符
    bool is_open_; // 是否打开

    bool configure(BaudRate baud_rate,
        DataBits data_bits,
        StopBits stop_bits,
        Parity parity);
    speed_t get_baud_rate_enum(BaudRate baud_rate);
    DataBits get_data_bits_enum(DataBits data_bits);
    StopBits get_stop_bits_enum(StopBits stop_bits);
    Parity get_parity_enum(Parity parity);
};

#endif // SERIAL_PORT_HPP
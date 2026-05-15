#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>



namespace Utils {
    // 获取当前时间戳 单位: 纳秒
    int64_t getCurrentTimestampNs();
    // 获取当前时间戳 单位: 微秒
    int64_t getCurrentTimestampUs();
    // 获取当前时间戳 单位: 毫秒
    int64_t getCurrentTimestampMs();
    // 获取当前时间戳 单位: 秒
    int64_t getCurrentTimestampSec();

    // 大端字节序转换为16位整数
    uint16_t be16(const uint8_t* p);
    // 大端字节序转换为32位整数
    uint32_t be32(const uint8_t* p);
    // 网络字节序转换为IP地址
    std::string ipFromNetOrder(uint32_t net);
    // IP地址转换为网络字节序
    uint32_t ipToHostOrder(const std::string& ip);
    // 解析字符串
    std::string parseStr(const uint8_t* p, size_t len);

    // 写入16位整数 大端字节序
    void wr16(uint8_t* p, uint16_t v);
    // 写入32位整数 大端字节序
    void wr32(uint8_t* p, uint32_t v);
    // 写入64位整数 大端字节序
    void wr64(uint8_t* p, uint64_t v);
    // 读取16位整数 大端字节序
    uint16_t rd16(const uint8_t* p);
    // 读取32位整数 大端字节序
    uint32_t rd32(const uint8_t* p);
    // 把 str 拷进固定长度字段，空余补 0 大端字节序
    void wrStr(uint8_t* dst, size_t cap, const std::string& s);

    // 字节转成16进制字符串
    std::string toHexString(const uint8_t* p, size_t len);
}


#endif

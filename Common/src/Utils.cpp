#include "Utils.h"
#include <chrono>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <cctype>
#include <sstream>
#include <iomanip>




int64_t Utils::getCurrentTimestampNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int64_t Utils::getCurrentTimestampUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int64_t Utils::getCurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int64_t Utils::getCurrentTimestampSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}


uint16_t Utils::be16(const uint8_t* p) { 
    return ntohs(*reinterpret_cast<const uint16_t*>(p)); 
}

uint32_t Utils::be32(const uint8_t* p) { 
    return ntohl(*reinterpret_cast<const uint32_t*>(p)); 
}

std::string Utils::ipFromNetOrder(uint32_t net) {
    char buf[INET_ADDRSTRLEN];
    in_addr a; a.s_addr = net;
    return inet_ntop(AF_INET, &a, buf, sizeof(buf));
}

uint32_t Utils::ipToHostOrder(const std::string& ip) {
    return ntohl(inet_addr(ip.c_str()));
}

std::string Utils::parseStr(const uint8_t* p, size_t len) {
    std::string s(reinterpret_cast<const char*>(p), len);
    auto nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

void Utils::wr16(uint8_t* p, uint16_t v) { 
    v = htons(v); std::memcpy(p, &v, 2); 
}

void Utils::wr32(uint8_t* p, uint32_t v) { 
    v = htonl(v); std::memcpy(p, &v, 4); 
}

void Utils::wr64(uint8_t* p, uint64_t v) {
    for (int i = 7; i >= 0; --i) { p[7 - i] = uint8_t(v >> (i * 8)); }
}

uint16_t Utils::rd16(const uint8_t* p) { 
    uint16_t v; std::memcpy(&v, p, 2); 
    return ntohs(v); 
}

uint32_t Utils::rd32(const uint8_t* p) { 
    uint32_t v; std::memcpy(&v, p, 4); 
    return ntohl(v); 
}

void Utils::wrStr(uint8_t* dst, size_t cap, const std::string& s) {
    std::memset(dst, 0, cap);
    std::memcpy(dst, s.data(), std::min(cap - 1, s.size()));
}

std::string Utils::toHexString(const uint8_t* p, size_t len) {
    std::stringstream ss;
    for (size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(p[i]);
    }
    return ss.str();
}
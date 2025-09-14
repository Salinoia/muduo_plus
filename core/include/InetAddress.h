#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>

// 封装IPv4地址结构，方便操作IP和端口
class InetAddress {
public:
    // 通过字符串IP和数字端口构造，默认 127.0.0.1:0
    explicit InetAddress(std::string ip = "127.0.0.1", uint16_t port = 0);

    // 直接通过sockaddr_in结构体构造
    explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

    // 获取IP字符串（如"192.168.1.1"）
    std::string toIp() const;

    // 获取IP:Port字符串（如"192.168.1.1:80"）
    std::string toIpPort() const;

    // 获取端口号（主机字节序）
    uint16_t toPort() const;

    // 获取底层socket地址结构指针（用于系统调用）
    const sockaddr_in* getSockAddr() const { return &addr_; }

    // 设置socket地址结构（用于accept等场景）
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    sockaddr_in addr_;  // 实际存储的地址结构
};

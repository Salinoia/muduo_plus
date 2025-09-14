#pragma once
#include "NonCopyable.h"

class InetAddress;
// 封装socket fd
class Socket : NonCopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int getSocketFd() const { return this->sockfd_; }
    void bindAddress(const InetAddress& local_addr);
    void listen();
    int accept(InetAddress* peer_addr);
    void shutdownWrite();

    void setTcpNoDelay(bool on);  // 禁用Nagle算法
    void setReuseAddr(bool on);  // 地址重用
    void setReusePort(bool on);  // 端口重用（负载均衡）
    void setKeepAlive(bool on);  // 心跳检测，设职长连接

private:
    const int sockfd_;
};
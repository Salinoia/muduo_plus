#pragma once

#include <functional>

#include "Channel.h"
#include "NonCopyable.h"
#include "Socket.h"

class EventLoop;
class InetAddress;

class Acceptor : NonCopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();
    
    // 监听本地端口
    void listen();
    // 判断是否在监听
    bool listenning() const { return listenning_; }
    // 设置新连接的回调函数
    void setNewConnectionCallback(const NewConnectionCallback& cb) { NewConnectionCallback_ = cb; }

private:
    // ==== 核心组件 ====
    EventLoop* loop_;  // 所属事件循环（必须首位）
    Socket acceptSocket_;  // 监听套接字（依赖loop_）
    Channel acceptChannel_;  // 监听channel（依赖acceptSocket_）

    // ==== 运行时状态 ====
    bool listenning_;  // 监听状态标志

    // ==== 回调接口 ====
    NewConnectionCallback NewConnectionCallback_;  // 新连接到达回调

    // ==== 内部方法 ====
    void handleRead();  // 处理可读事件（接受新连接）
};
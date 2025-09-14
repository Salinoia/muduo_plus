#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "TimeStamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 **/

class TcpConnection : NonCopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    void send(const std::string& buf);
    void sendFile(int fileDescriptor, off_t offset, size_t count);

    // 关闭半连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE {
        kDisconnected,  // 已经断开连接
        kConnecting,  // 正在连接
        kConnected,  // 已连接
        kDisconnecting  // 正在断开连接
    };
    // ==== 核心状态与组件 ====
    EventLoop* loop_;  // 若为多Reactor 该loop_指向subloop；若为单Reactor 该loop_指向baseloop；
    std::atomic_int state_;  // 连接状态，与loop_强相关
    bool reading_;  // 连接是否在监听读事件

    // ==== 网络资源 ====
    // Socket Channel
    // 和Acceptor类似    Acceptor => mainloop    TcpConnection => subloop
    std::unique_ptr<Socket> socket_;  // TCP套接字（核心资源）
    std::unique_ptr<Channel> channel_;  // 事件通道

    // ==== 地址信息 ====
    const std::string name_;  // 连接名称
    const InetAddress localAddr_;  // 本地地址
    const InetAddress peerAddr_;  // 对端地址

    // ==== 数据缓冲区 ====
    Buffer inputBuffer_;               // 接收缓冲区（高频访问）
    Buffer outputBuffer_;              // 发送缓冲区

    // ==== 水位控制 ====
    size_t highWaterMark_;             // 高水位阈值
    HighWaterMarkCallback highWaterMarkCallback_;// 高水位回调

    // ==== 用户回调 ==== 
    // 用户通过写入TcpServer注册 TcpServer再将注册的回调传递给TcpConnection TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;  // 有新连接时的回调
    MessageCallback messageCallback_;  // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成以后的回调
    CloseCallback closeCallback_;  // 关闭连接的回调

    // ==== 内部方法 ====
    void setState(StateE state) { state_ = state; }
    void handleRead(TimeStamp receiveTime);
    void handleWrite();  // 处理写事件
    void handleClose();
    void handleError();
    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
};

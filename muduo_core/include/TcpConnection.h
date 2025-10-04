#pragma once

#include <any>
#include <atomic>
#include <memory>
#include <string>

#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "Timestamp.h"

class Channel;
class Socket;

/**
 * TcpConnection: 表示一次TCP连接（主动或被动）
 * 生命周期由 shared_ptr<TcpConnection> 管理
 * 在 Muduo 的多Reactor模型下，每个连接隶属于某个 subloop
 */
class TcpConnection : NonCopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    // ========== 数据发送接口 ==========
    void send(const std::string& buf);
    void send(const void* data, size_t len);
    void send(Buffer* buf);  // 对齐 HttpServer.cpp 中的调用

    void sendFile(int fileDescriptor, off_t offset, size_t count);

    // 关闭写端（半关闭）
    void shutdown();

    // ========== 回调注册接口 ==========
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    // ========== 生命周期接口 ==========
    void connectEstablished();  // 由 TcpServer 在新连接 accept 后调用
    void connectDestroyed();  // 由 TcpServer 在连接关闭时调用

    // ========== 用户上下文存取（支持 TLS / HTTP 复用）==========
    void setContext(const std::any& context) { context_ = context; }
    const std::any& getContext() const { return context_; }
    std::any& getMutableContext() { return context_; }
    void clearContext() { context_.reset(); }

private:
    enum StateE {
        kDisconnected,  // 已断开
        kConnecting,  // 正在连接
        kConnected,  // 已连接
        kDisconnecting  // 正在断开
    };

    void setState(StateE s) { state_ = s; }

    // ========== 内部事件回调 ==========
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    // ========== 内部执行函数 ==========
    void sendInLoop(const void* data, size_t len);
    void sendFileInLoop(int fileDescriptor, off_t offset, size_t count);
    void shutdownInLoop();

private:
    EventLoop* loop_;  // 所属事件循环（线程）
    std::atomic_int state_;  // 状态机
    bool reading_;  // 是否监听读事件

    std::unique_ptr<Socket> socket_;  // 封装的 socket
    std::unique_ptr<Channel> channel_;  // 绑定事件通道

    const std::string name_;  // 连接名
    const InetAddress localAddr_;  // 本地地址
    const InetAddress peerAddr_;  // 对端地址

    Buffer inputBuffer_;  // 输入缓冲
    Buffer outputBuffer_;  // 输出缓冲

    size_t highWaterMark_;  // 高水位阈值
    HighWaterMarkCallback highWaterMarkCallback_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;

    // 任意类型上下文，支持 TLSConnection / HttpContext / 用户对象
    std::any context_;
};

namespace std {
template <>
struct formatter<TcpConnection*, char> : formatter<void*, char> {
    auto format(TcpConnection* p, auto& ctx) const { return formatter<void*, char>::format(static_cast<void*>(p), ctx); }
};
}  // namespace std

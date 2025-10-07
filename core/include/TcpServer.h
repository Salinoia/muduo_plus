#pragma once

/**
 * 用户使用muduo编写服务器程序
 **/

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "TcpConnection.h"

// 对外的服务器编程使用的类
class TcpServer {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option {
        kNoReusePort,  // 不允许重用本地端口
        kReusePort,  // 允许重用本地端口
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);
    /**
     * 如果没有监听, 就启动服务器(监听).
     * 多次调用没有副作用.
     * 线程安全.
     */
    void start();

private:
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    // ==== 核心组件 ====
    EventLoop* loop_;  // Main Reactor（必须首位）
    const std::string ipPort_;  // 监听地址（格式 "IP:PORT"）
    const std::string name_;  // 服务名称

    // ==== 网络资源 ====
    std::unique_ptr<Acceptor> acceptor_;  // 连接接收器（主循环）
    std::shared_ptr<EventLoopThreadPool> threadPool_;  // 线程池

    // ==== 连接管理 ====
    ConnectionMap connections_;  // 活跃连接表（核心状态）
    std::atomic_int nextConnId_;  // 连接ID生成器

    // ==== 配置参数 ====
    int numThreads_;  // 子线程数
    std::atomic_int started_;  // 启动状态标志

    // ==== 用户回调 ====
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;

    // ==== 内部方法 ====
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);
};
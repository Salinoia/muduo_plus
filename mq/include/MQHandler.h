#pragma once
#include <openssl/ssl.h>  // 必须：提供 SSL/SSL_CTX
#include <amqpcpp.h>

#include <amqpcpp/linux_tcp/tcpparent.h>    // 先于 tcpconnection
#include <amqpcpp/linux_tcp/tcphandler.h>
#include <amqpcpp/linux_tcp/tcpconnection.h>
#include <amqpcpp/linux_tcp/tcpchannel.h>

#include <string>

#include "Channel.h" 
#include "EventLoop.h"
#include "LogMacros.h"
// 将 AMQP-CPP 的 TcpHandler 事件对接到 core 的 EventLoop/Channel
class MQHandler : public AMQP::TcpHandler {
public:
    explicit MQHandler(EventLoop* loop) : loop_(loop) {}

    // 通知我们需监听哪个 fd 的哪些事件（读/写）
    void monitor(AMQP::TcpConnection* connection, int fd, int flags) override {
        if (!channel_) {
            connFd_ = fd;
            channel_ = std::make_unique<Channel>(loop_, fd);
            channel_->setReadCallback([this, connection](Timestamp) {
                // 触发 AMQP-CPP 处理可读事件
                connection->process(connFd_, AMQP::readable);
            });

            channel_->setWriteCallback([this, connection] {
                // 触发 AMQP-CPP 处理可写事件
                connection->process(connFd_, AMQP::writable);
            });
        }

        // 根据 flags 开关读/写监听
        if (flags & AMQP::readable)
            channel_->enableReading();
        else
            channel_->disableReading();
        if (flags & AMQP::writable)
            channel_->enableWriting();
        else
            channel_->disableWriting();
    }

    // 连接生命周期回调（可用于日志/度量）
    void onConnected(AMQP::TcpConnection*) override { LOG_INFO("[MQ] Connected to RabbitMQ."); }
    void onClosed(AMQP::TcpConnection*) override { LOG_INFO("[MQ] Connection closed."); }
    void onError(AMQP::TcpConnection*, const char* msg) override { LOG_ERROR("[MQ] Error: ", msg); }

private:
    EventLoop* loop_{nullptr};
    std::unique_ptr<Channel> channel_;
    int connFd_{-1};
};
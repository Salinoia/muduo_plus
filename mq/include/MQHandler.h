#pragma once
#include <openssl/ssl.h>  // 必须：提供 SSL/SSL_CTX
#include <amqpcpp.h>
#include <amqpcpp/linux_tcp/tcpparent.h>  // 先于 tcpconnection
#include <amqpcpp/linux_tcp/tcphandler.h>
#include <amqpcpp/linux_tcp/tcpconnection.h>
#include <amqpcpp/linux_tcp/tcpchannel.h>
#include <fcntl.h>

#include <atomic>
#include <string>

#include "Channel.h"
#include "EventLoop.h"
#include "LogMacros.h"
// 将 AMQP-CPP 的 TcpHandler 事件对接到 core 的 EventLoop/Channel
class MQHandler : public AMQP::TcpHandler {
public:
    explicit MQHandler(EventLoop* loop) : loop_(loop) {}

    void unregister() {
        if (closed_.exchange(true))
            return;

        Channel* ch = channel_.release();  // 所有权交出，必须由 loop 线程销毁
        if (!ch)
            return;

        connFd_ = -1;
        loop_->queueInLoop([ch]() {
            // 禁止调用 ch->disableAll()，它会先触发 EPOLL_CTL_DEL：
            // 如果 fd 已经被对端/库关闭，就会产生 EBADF 垃圾日志。
            // 最后的关闭时延存在 30 s，是 AMQP 优雅关闭必然的时延，应用层无法修改

            ch->remove();  // 直接从 poller 移除（见下：EPollPoller 忽略 EBADF/ENOENT）
            delete ch;  // 最终销毁
        });
    }


    void monitor(AMQP::TcpConnection* connection, int fd, int flags) override {
        // flags==0 表示连接将关闭或已关闭 —— 只做一次 unregister，然后忽略后续回调
        if (flags == 0) {
            unregister();
            return;
        }

        // 若已进入关闭态，丢弃任何后续的 monitor 信号（避免在已关闭 fd 上 add/mod）
        if (closed_.load(std::memory_order_acquire))
            return;

        if (!channel_) {
            connFd_ = fd;
            channel_ = std::make_unique<Channel>(loop_, fd);

            // 这些回调可能在 epoll 事件里被触发，里面又会间接导致 monitor()
            channel_->setReadCallback([this, connection](Timestamp) {
                // 如果已经关闭，避免再触发 AMQP 的 process（可能又回调 monitor）
                if (!closed_.load(std::memory_order_acquire))
                    connection->process(connFd_, AMQP::readable);
            });
            channel_->setWriteCallback([this, connection] {
                if (!closed_.load(std::memory_order_acquire))
                    connection->process(connFd_, AMQP::writable);
            });
        }

        // 根据 flags 切换事件，但要在“未关闭”前提下做
        if (flags & AMQP::readable)
            channel_->enableReading();
        else
            channel_->disableReading();
        if (flags & AMQP::writable)
            channel_->enableWriting();
        else
            channel_->disableWriting();
    }

    void onConnected(AMQP::TcpConnection*) override { LOG_INFO("[MQ] Connected to RabbitMQ."); }
    void onClosed(AMQP::TcpConnection*) override { LOG_INFO("[MQ] Connection closed."); }
    void onError(AMQP::TcpConnection*, const char* msg) override { LOG_ERROR("[MQ] Error: {}", msg); }

private:
    EventLoop* loop_{nullptr};
    std::unique_ptr<Channel> channel_;
    int connFd_{-1};

    // 关键：标记“已经进入关闭流程”，防止 monitor/回调在关闭后继续操作 fd / channel
    std::atomic<bool> closed_{false};
};

#pragma once

#include <memory>
#include <string>

#include "MQHandler.h"

// 负责创建 MQHandler 与 AMQP::TcpConnection
class MQClient {
public:
    MQClient(EventLoop* loop, const std::string& url);
    ~MQClient();

    AMQP::TcpConnection* connection() const { return connection_.get(); }
    EventLoop* loop() const { return loop_; }

private:
    EventLoop* loop_{nullptr};
    std::unique_ptr<MQHandler> handler_;
    std::unique_ptr<AMQP::TcpConnection> connection_;
};

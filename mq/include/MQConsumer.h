#pragma once

#include "MQClient.h"

#include <functional>
#include <memory>
#include <string>
#include <amqpcpp.h>
#include <amqpcpp/linux_tcp/tcpchannel.h>


// 异步消费者封装
class MQConsumer {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    explicit MQConsumer(MQClient* client);

    // 订阅队列：收到消息后回调 cb(body)
    void consume(const std::string& queue, MessageCallback cb);

private:
    std::unique_ptr<AMQP::TcpChannel> channel_;
};

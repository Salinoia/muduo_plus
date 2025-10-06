#pragma once

#include "MQClient.h"

#include <amqpcpp.h>
#include <amqpcpp/linux_tcp/tcpchannel.h>

#include <memory>
#include <string>


// 异步发布者封装
class MQProducer {
public:
    explicit MQProducer(MQClient* client);

    // exchange 可为 ""（直连到队列），routingKey 为队列名
    void publish(const std::string& exchange, const std::string& routingKey, const std::string& message);

private:
    std::unique_ptr<AMQP::TcpChannel> channel_;
};

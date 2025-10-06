#include "MQConsumer.h"

#include "LogMacros.h"

MQConsumer::MQConsumer(MQClient* client) {
    channel_ = std::make_unique<AMQP::TcpChannel>(client->connection());

    // 可选：错误回调
    channel_->onError([](const char* msg) { LOG_ERROR("[MQConsumer] Channel error: ", msg); });
}

void MQConsumer::consume(const std::string& queue, MessageCallback cb) {
    // 声明队列（幂等），再开始消费
    channel_->declareQueue(queue, AMQP::durable).onSuccess([queue] { LOG_INFO("[MQConsumer] Declared queue: {}", queue); });

    channel_->consume(queue, AMQP::noack).onReceived([cb](const AMQP::Message& msg, uint64_t, bool) {
        std::string body(msg.body(), msg.bodySize());
        cb(body);
    });
}

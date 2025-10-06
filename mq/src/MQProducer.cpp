#include "MQProducer.h"

#include "LogMacros.h"

MQProducer::MQProducer(MQClient* client) {
    channel_ = std::make_unique<AMQP::TcpChannel>(client->connection());

    // 可选：错误回调
    channel_->onError([](const char* msg) { LOG_ERROR("[MQProducer] Channel error: {}", msg); });
}

void MQProducer::publish(const std::string& exchange, const std::string& routingKey, const std::string& message) {
    // 若 exchange 为空，则按直连到队列的语义（默认交换机）
    channel_->publish(exchange, routingKey, message);
    LOG_INFO("[MQProducer] Published to [{}] key=[{}] size={}", exchange, routingKey, message.size());
}

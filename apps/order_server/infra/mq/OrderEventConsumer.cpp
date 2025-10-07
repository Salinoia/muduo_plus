#include "infra/mq/OrderEventConsumer.h"

#include <utility>
#include <iostream>

#include "MQConsumer.h"

// ========== 构造函数 ==========

OrderEventConsumer::OrderEventConsumer(Dependencies deps) : OrderEventConsumer(std::move(deps), Options{}) {}

OrderEventConsumer::OrderEventConsumer(Dependencies deps, Options options) : deps_(std::move(deps)), options_(std::move(options)) {}

// ========== 生命周期管理 ==========

void OrderEventConsumer::Start(RawHandler handler) {
    if (running_)
        return;
    if (!deps_.mq) {
        std::cerr << "[OrderEventConsumer] Missing MQConsumer dependency\n";
        return;
    }
    handler_ = std::move(handler);
    running_ = true;

    deps_.mq->consume(options_.queueName, [this](const std::string& payload) { handleMessage(payload); });

    std::cout << "[OrderEventConsumer] Started consuming queue: " << options_.queueName << std::endl;
}

void OrderEventConsumer::Stop() {
    if (!running_)
        return;
    running_ = false;
    handler_ = nullptr;
    std::cout << "[OrderEventConsumer] Stopped consuming queue: " << options_.queueName << std::endl;
}

// ========== 消息分发 ==========

void OrderEventConsumer::handleMessage(const std::string& payload) {
    if (!running_)
        return;
    if (handler_) {
        try {
            handler_(payload);
        } catch (const std::exception& e) {
            std::cerr << "[OrderEventConsumer] Handler exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[OrderEventConsumer] Unknown handler exception\n";
        }
    }
}

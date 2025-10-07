#include "interface/mq/MQEventRouter.h"

#include <iostream>
#include <sstream>
#include <utility>

#include "infra/mq/OrderEventConsumer.h"
#include "domain/OrderService.h"
#include "domain/InventoryService.h"

// ========== 构造与初始化 ==========

MQEventRouter::MQEventRouter(Dependencies deps) : MQEventRouter(std::move(deps), Options{}) {}

MQEventRouter::MQEventRouter(Dependencies deps, Options options) : deps_(std::move(deps)), options_(std::move(options)) {}

void MQEventRouter::Initialize() {
    // 注册事件类型与处理函数
    registerHandler("order.created", [this](const std::string& payload) { onOrderCreated(payload); });
    registerHandler("order.paid", [this](const std::string& payload) { onOrderPaid(payload); });
    registerHandler("order.cancelled", [this](const std::string& payload) { onOrderCancelled(payload); });
    registerHandler("inventory.released", [this](const std::string& payload) { onInventoryReleased(payload); });

    if (options_.enableLogging) {
        std::cout << "[MQEventRouter] Initialized with " << handlers_.size() << " handlers." << std::endl;
    }
}

// ========== 启动与停止 ==========

void MQEventRouter::Start() {
    if (running_)
        return;
    if (!deps_.consumer) {
        std::cerr << "[MQEventRouter] Missing OrderEventConsumer dependency." << std::endl;
        return;
    }

    running_ = true;
    deps_.consumer->Start([this](const std::string& payload) { routeMessage(payload); });

    if (options_.enableLogging) {
        std::cout << "[MQEventRouter] Started routing MQ events." << std::endl;
    }
}

void MQEventRouter::Stop() {
    if (!running_)
        return;
    running_ = false;
    if (deps_.consumer)
        deps_.consumer->Stop();

    if (options_.enableLogging) {
        std::cout << "[MQEventRouter] Stopped routing MQ events." << std::endl;
    }
}

// ========== 消息路由核心逻辑 ==========

void MQEventRouter::routeMessage(const std::string& payload) {
    // 这里假设消息格式非常简单：
    // {"event":"order.paid","orderId":"12345"}
    // 为避免引入 JSON 库，这里用极简字符串解析（仅用于演示）
    std::string event;
    auto pos = payload.find("\"event\"");
    if (pos != std::string::npos) {
        auto start = payload.find(':', pos);
        auto quote1 = payload.find('"', start + 1);
        auto quote2 = payload.find('"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos)
            event = payload.substr(quote1 + 1, quote2 - quote1 - 1);
    }

    if (event.empty()) {
        std::cerr << "[MQEventRouter] Invalid message: no event field." << std::endl;
        return;
    }

    auto it = handlers_.find(event);
    if (it == handlers_.end()) {
        std::cerr << "[MQEventRouter] No handler registered for event: " << event << std::endl;
        return;
    }

    if (options_.enableLogging) {
        std::cout << "[MQEventRouter] Dispatching event: " << event << std::endl;
    }

    try {
        it->second(payload);
    } catch (const std::exception& e) {
        std::cerr << "[MQEventRouter] Handler exception for " << event << ": " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[MQEventRouter] Unknown handler exception for " << event << std::endl;
    }
}

void MQEventRouter::registerHandler(std::string_view eventType, Handler handler) {
    handlers_.emplace(std::string(eventType), std::move(handler));
}

// ========== 事件处理函数 ==========

void MQEventRouter::onOrderCreated(const std::string& payload) {
    if (!deps_.orders)
        return;
    if (options_.enableLogging)
        std::cout << "[MQEventRouter] onOrderCreated: " << payload << std::endl;
    // 在这里可以调用 OrderService::RefreshCache 或其他操作
}

void MQEventRouter::onOrderPaid(const std::string& payload) {
    if (!deps_.orders)
        return;
    if (options_.enableLogging)
        std::cout << "[MQEventRouter] onOrderPaid: " << payload << std::endl;
    // 解析 orderId / amount / paidAt 并调用 OrderService::MarkPaid()
}

void MQEventRouter::onOrderCancelled(const std::string& payload) {
    if (!deps_.orders)
        return;
    if (options_.enableLogging)
        std::cout << "[MQEventRouter] onOrderCancelled: " << payload << std::endl;
    // 调用 OrderService::CancelOrder()
}

void MQEventRouter::onInventoryReleased(const std::string& payload) {
    if (!deps_.inventory)
        return;
    if (options_.enableLogging)
        std::cout << "[MQEventRouter] onInventoryReleased: " << payload << std::endl;
    // 调用 InventoryService::ReleaseReservation() 或 AdjustStock()
}

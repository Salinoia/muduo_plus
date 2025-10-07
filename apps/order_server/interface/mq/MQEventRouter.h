#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <string_view>

#include "infra/mq/OrderEventConsumer.h"
#include "domain/OrderService.h"
#include "domain/InventoryService.h"

/**
 * @brief MQEventRouter：消息事件路由器
 *
 * interface 层组件，订阅 MQ 事件并调用领域服务执行业务操作。
 * 不直接依赖 MQ 底层，仅依赖 infra 层封装的 OrderEventConsumer。
 */
class MQEventRouter {
public:
    struct Dependencies {
        OrderEventConsumer* consumer{nullptr};
        OrderService* orders{nullptr};
        InventoryService* inventory{nullptr};
    };

    struct Options {
        bool enableLogging{true};
    };

    using Handler = std::function<void(const std::string&)>;

    MQEventRouter(Dependencies deps);
    MQEventRouter(Dependencies deps, Options options);

    void Initialize();  // 注册所有事件处理函数
    void Start();  // 启动消费
    void Stop();  // 停止消费

private:
    void routeMessage(const std::string& payload);
    void registerHandler(std::string_view eventType, Handler handler);

    // 示例事件处理
    void onOrderCreated(const std::string& payload);
    void onOrderPaid(const std::string& payload);
    void onOrderCancelled(const std::string& payload);
    void onInventoryReleased(const std::string& payload);

private:
    Dependencies deps_;
    Options options_;
    std::unordered_map<std::string, Handler> handlers_;
    bool running_{false};
};

#pragma once

#include <functional>
#include <string>
#include <string_view>

class MQConsumer;

/**
 * @brief OrderEventConsumer：订单 MQ 消费器基础设施封装
 *
 * 提供 Start/Stop 生命周期管理，并将消息分发给上层回调。
 */
class OrderEventConsumer {
public:
    struct Dependencies {
        MQConsumer* mq{nullptr};
    };

    struct Options {
        std::string queueName{"order.events"};
        bool autoAck{true};
    };

    using RawHandler = std::function<void(const std::string& payload)>;

    OrderEventConsumer(Dependencies deps);
    OrderEventConsumer(Dependencies deps, Options options);

    void Start(RawHandler handler);
    void Stop();

    bool IsRunning() const noexcept { return running_; }
    const Options& options() const noexcept { return options_; }

private:
    void handleMessage(const std::string& payload);

private:
    Dependencies deps_;
    Options options_;
    RawHandler handler_;
    bool running_{false};
};

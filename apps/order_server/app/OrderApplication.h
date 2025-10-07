#pragma once
#include "OrderConfig.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "HttpServer.h"
#include "InetAddress.h"
#include "MySQLConnInfo.h"
#include "NonCopyable.h"


// 前置声明：解耦具体实现
class EventLoop;
class RedisPool;
class MySQLConnPool;
class MQClient;
class MQConsumer;
class MQProducer;
class OrderCache;
class OrderRepository;
class OrderCreateHandler;
class OrderQueryHandler;
class OrderService;
class OrderEventConsumer;
class InventoryService;
class MQEventRouter;

/**
 * @brief OrderApplication：订单服务主协调器
 *
 * 功能职责：
 *  1. 启动并管理 HTTP 服务（RESTful 接口）
 *  2. 初始化 MySQL / Redis 连接池及相关资源
 *  3. 启动 MQ 消费者并分发异步任务
 *  4. 调度各业务 Handler（下单、查询、库存）
 *
 * 架构定位：
 *  作为系统的“业务协调层”，衔接通信层（Reactor）、存储层（DB/Cache）和消息层（MQ）。
 */
class OrderApplication : public NonCopyable {
public:
    using Options = OrderServerOptions;  // 使用统一配置结构（可从 YAML 载入）

    // ===== 构造与析构 =====
    explicit OrderApplication(EventLoop* loop, const InetAddress& listenAddr, Options options = Options());
    ~OrderApplication();

    // ===== 生命周期控制 =====
    void start();  // 启动整个服务（幂等）
    bool isStarted() const noexcept { return started_; }

    // ===== 对外扩展接口 =====
    MySQLConnPool* mysqlPool() const noexcept { return mysqlPool_.get(); }
    RedisPool* redisPool() const noexcept { return redisPool_.get(); }
    OrderCache* cache() const noexcept { return orderCache_.get(); }
    OrderRepository* database() const noexcept { return orderRepository_.get(); }
    OrderService* orderService() const noexcept { return orderService_.get(); }
    OrderEventConsumer* consumer() const noexcept { return orderConsumer_.get(); }
    InventoryService* inventory() const noexcept { return inventoryService_.get(); }
    OrderCreateHandler* createHandler() const noexcept { return createHandler_.get(); }
    OrderQueryHandler* queryHandler() const noexcept { return queryHandler_.get(); }

private:
    // ===== 内部初始化阶段 =====
    void configureHttpServer();  // 设置 HTTP 线程数 / TLS 等
    void initStorage();  // 初始化 Redis / MySQL
    void initHandlers();  // 注册业务 Handler
    void initRoutes();  // 构造 HTTP 路由表
    void initMessageQueue();  // 初始化 MQ 客户端与消费者
    void warmupCache();  // 预热缓存（可选）

private:
    // ===== 核心组件 =====
    EventLoop* loop_;  // 主事件循环（非拥有）
    HttpServer httpServer_;  // HTTP 服务主机
    Options options_;  // 服务配置（来自 YAML 或默认）

    // ===== 中间件与资源层 =====
    std::shared_ptr<MySQLConnPool> mysqlPool_;
    std::shared_ptr<RedisPool> redisPool_;
    std::unique_ptr<MQClient> mqClient_;
    std::unique_ptr<MQProducer> mqProducer_;
    std::unique_ptr<MQConsumer> mqConsumer_;
    std::unique_ptr<OrderEventConsumer> orderConsumer_;

    // ===== 数据与业务逻辑层 =====
    std::unique_ptr<OrderCache> orderCache_;
    std::unique_ptr<OrderRepository> orderRepository_;
    std::unique_ptr<OrderService> orderService_;
    std::unique_ptr<InventoryService> inventoryService_;
    std::unique_ptr<MQEventRouter> mqRouter_;
    std::unique_ptr<OrderCreateHandler> createHandler_;
    std::unique_ptr<OrderQueryHandler> queryHandler_;

    bool started_{false};  // 防止重复启动
};

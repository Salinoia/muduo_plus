#include "app/OrderApplication.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "LogMacros.h"
#include "MQClient.h"
#include "MQConsumer.h"
#include "MQProducer.h"
#include "MySQLConnPool.h"
#include "RedisPool.h"

#include "domain/InventoryService.h"
#include "domain/OrderService.h"
#include "infra/cache/OrderCache.h"
#include "infra/db/OrderRepository.h"
#include "infra/mq/OrderEventConsumer.h"
#include "interface/http/OrderCreateHandler.h"
#include "interface/http/OrderQueryHandler.h"
#include "interface/mq/MQEventRouter.h"

namespace {

std::size_t GetThreadCount(unsigned int configured) {
    if (configured > 0)
        return configured;
    auto hc = std::thread::hardware_concurrency();
    return hc == 0 ? 1 : hc;
}

std::string GenerateOrderId() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return "ORD-" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

}  // namespace

OrderApplication::OrderApplication(EventLoop* loop, const InetAddress& listenAddr, Options options) :
    loop_(loop), httpServer_(loop, listenAddr, options.serviceName, options.enableTLS), options_(std::move(options)) {}

OrderApplication::~OrderApplication() {
    if (mqRouter_)
        mqRouter_->Stop();
    if (orderConsumer_ && orderConsumer_->IsRunning())
        orderConsumer_->Stop();
    started_ = false;
}

void OrderApplication::start() {
    if (started_)
        return;
    if (!loop_) {
        throw std::runtime_error("OrderApplication requires a valid EventLoop");
    }

    configureHttpServer();
    initStorage();
    initMessageQueue();
    initHandlers();
    initRoutes();
    warmupCache();

    httpServer_.start();
    if (mqRouter_)
        mqRouter_->Start();

    started_ = true;
    LOG_INFO("OrderApplication started successfully");
}

void OrderApplication::configureHttpServer() {
    const auto threads = static_cast<int>(GetThreadCount(options_.httpThreadNum));
    httpServer_.setThreadNum(threads);

    httpServer_.setHttpCallback([](const HttpRequest&, HttpResponse* resp) {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setContentType("application/json");
        resp->setBody("{\"error\":\"Not Found\"}");
    });
}

void OrderApplication::initStorage() {
    if (!options_.database.validate())
        throw std::runtime_error("Invalid database configuration");

    // MySQL
    mysqlPool_ = MySQLConnPool::GetInstance(options_.database.connInfo.database);
    mysqlPool_->InitPool(options_.database.connInfo, 4, 16, 60, options_.database.connInfo.timeout_sec);

    orderRepository_ = std::make_unique<OrderRepository>(mysqlPool_, "orders");
    orderRepository_->EnsureSchema();

    // Redis
    if (!options_.redis.validate())
        throw std::runtime_error("Invalid redis configuration");

    redisPool_ = std::make_shared<RedisPool>(options_.redis.host, options_.redis.port, options_.redis.poolSize, options_.redis.password, options_.redis.timeoutMs);

    OrderCache::Options cacheOptions;
    cacheOptions.keyPrefix = options_.redis.keyPrefix + "detail:";
    cacheOptions.userIndexPrefix = options_.redis.keyPrefix + "user:";
    orderCache_ = std::make_unique<OrderCache>(redisPool_, cacheOptions);
}

void OrderApplication::initHandlers() {
    if (!orderRepository_)
        throw std::runtime_error("Order repository not initialized");

    // Inventory service depends on Redis and MQ (optional)
    InventoryService::Dependencies invDeps;
    invDeps.redis = redisPool_.get();
    invDeps.producer = mqProducer_.get();
    invDeps.orders = orderRepository_.get();

    inventoryService_ = std::make_unique<InventoryService>(invDeps, InventoryService::Options{});

    OrderService::Dependencies orderDeps;
    orderDeps.database = orderRepository_.get();
    orderDeps.cache = orderCache_.get();
    orderDeps.inventory = inventoryService_.get();
    orderDeps.producer = mqProducer_.get();

    orderService_ = std::make_unique<OrderService>(orderDeps);

    OrderCreateHandler::Dependencies createDeps;
    createDeps.database = orderRepository_.get();
    createDeps.cache = orderCache_.get();
    createDeps.inventory = inventoryService_.get();
    createDeps.producer = mqProducer_.get();

    OrderCreateHandler::Options createOpts;
    createOpts.mqExchange.clear();
    createOpts.mqRoutingKey = options_.mq.orderQueue;
    createOpts.enableCache = static_cast<bool>(orderCache_);
    createOpts.enableMqPublish = static_cast<bool>(mqProducer_);
    createOpts.requireInventoryReservation = true;

    createHandler_ = std::make_unique<OrderCreateHandler>(createDeps, createOpts);
    createHandler_->setIdGenerator(GenerateOrderId);

    OrderQueryHandler::Dependencies queryDeps;
    queryDeps.database = orderRepository_.get();
    queryDeps.cache = orderCache_.get();

    OrderQueryHandler::Options queryOpts;
    queryOpts.preferCache = static_cast<bool>(orderCache_);
    queryOpts.maxPageSize = orderService_->options().maxPageSize;
    queryHandler_ = std::make_unique<OrderQueryHandler>(queryDeps, queryOpts);

    if (mqConsumer_) {
        orderConsumer_ = std::make_unique<OrderEventConsumer>(OrderEventConsumer::Dependencies{mqConsumer_.get()}, OrderEventConsumer::Options{options_.mq.orderQueue, true});

        MQEventRouter::Dependencies deps;
        deps.consumer = orderConsumer_.get();
        deps.orders = orderService_.get();
        deps.inventory = inventoryService_.get();

        MQEventRouter::Options routerOpts;
        routerOpts.enableLogging = true;

        mqRouter_ = std::make_unique<MQEventRouter>(deps, routerOpts);
        mqRouter_->Initialize();
    }
}

void OrderApplication::initRoutes() {
    if (createHandler_) {
        auto handlerPtr = std::shared_ptr<OrderCreateHandler>(createHandler_.get(), [](OrderCreateHandler*) {});
        httpServer_.Post("/orders", handlerPtr);
    }

    if (queryHandler_) {
        auto handlerPtr = std::shared_ptr<OrderQueryHandler>(queryHandler_.get(), [](OrderQueryHandler*) {});
        httpServer_.Get("/orders", handlerPtr);
    }
}

void OrderApplication::initMessageQueue() {
    if (!options_.mq.validate()) {
        LOG_WARN("MQ configuration invalid, skipping MQ initialization");
        return;
    }

    mqClient_ = std::make_unique<MQClient>(loop_, options_.mq.url);
    mqProducer_ = std::make_unique<MQProducer>(mqClient_.get());
    mqConsumer_ = std::make_unique<MQConsumer>(mqClient_.get());
}

void OrderApplication::warmupCache() {
    if (!orderCache_ || !orderRepository_ || !orderService_)
        return;

    try {
        auto recent = orderRepository_->ListRecent(20);
        if (!recent.empty()) {
            orderCache_->Warmup(recent);
            LOG_INFO("Cache warmup completed with {} orders", recent.size());
        }
    } catch (const std::exception& e) {
        LOG_WARN("Cache warmup failed: {}", e.what());
    }
}

#include "OrderConfig.h"
#include "ConfigLoader.hpp"
#include <thread>

OrderServerOptions OrderServerOptions::FromConfig(const std::string& path) {
    std::cerr << "[Debug] creating ConfigLoader" << std::endl;
    ConfigLoader cfg(path);
    OrderServerOptions opt;

    // --------------------------- 基础 ---------------------------
    opt.serviceName = cfg.get("serviceName", opt.serviceName);
    opt.enableTLS = cfg.get("enableTLS", opt.enableTLS);
    opt.httpThreadNum = cfg.get("httpThreadNum", std::max(1u, std::thread::hardware_concurrency()));

    // --------------------------- Database ---------------------------
    auto& db = opt.database;
    db.connInfo.url = cfg.getPath("database.connInfo.url", db.connInfo.url);
    db.connInfo.user = cfg.getPath("database.connInfo.user", db.connInfo.user);
    db.connInfo.password = cfg.getPath("database.connInfo.password", db.connInfo.password);
    db.connInfo.database = cfg.getPath("database.connInfo.database", db.connInfo.database);
    db.connInfo.timeout_sec = cfg.getPath("database.connInfo.timeout_sec", db.connInfo.timeout_sec);
    db.maxConnections = cfg.getPath("database.maxConnections", db.maxConnections);
    db.minConnections = cfg.getPath("database.minConnections", db.minConnections);
    db.maxIdleTime = cfg.getPath("database.maxIdleTime", db.maxIdleTime);
    db.connectTimeout = cfg.getPath("database.connectTimeout", db.connectTimeout);

    // --------------------------- Redis ---------------------------
    auto& redis = opt.redis;
    redis.host = cfg.getPath("redis.host", redis.host);
    redis.port = cfg.getPath("redis.port", redis.port);
    redis.password = cfg.getPath("redis.password", redis.password);
    redis.poolSize = cfg.getPath("redis.poolSize", redis.poolSize);
    redis.timeoutMs = cfg.getPath("redis.timeoutMs", redis.timeoutMs);
    redis.keyPrefix = cfg.getPath("redis.keyPrefix", redis.keyPrefix);
    redis.enableCache = cfg.getPath("redis.enableCache", redis.enableCache);

    // --------------------------- MQ ---------------------------
    auto& mq = opt.mq;
    mq.url = cfg.getPath("mq.url", mq.url);
    mq.orderQueue = cfg.getPath("mq.orderQueue", mq.orderQueue);
    mq.inventoryQueue = cfg.getPath("mq.inventoryQueue", mq.inventoryQueue);
    mq.exchange = cfg.getPath("mq.exchange", mq.exchange);
    mq.enableConsumer = cfg.getPath("mq.enableConsumer", mq.enableConsumer);

    // --------------------------- Metrics ---------------------------
    auto& metrics = opt.metrics;
    metrics.enablePrometheus = cfg.getPath("metrics.enablePrometheus", metrics.enablePrometheus);
    metrics.port = cfg.getPath("metrics.port", metrics.port);

    // --------------------------- Logging ---------------------------
    auto& log = opt.logging;
    log.level = cfg.getPath("logging.level", log.level);
    log.console = cfg.getPath("logging.console", log.console);
    log.file = cfg.getPath("logging.file", log.file);

    // --------------------------- Reservation ---------------------------
    auto& resv = opt.reservation;
    resv.ttl_seconds = cfg.getPath("reservation.ttl_seconds", resv.ttl_seconds);
    resv.restockRoutingKey = cfg.getPath("reservation.restockRoutingKey", resv.restockRoutingKey);
    resv.reservationRoutingKey = cfg.getPath("reservation.reservationRoutingKey", resv.reservationRoutingKey);

    // --------------------------- Cache ---------------------------
    auto& cache = opt.cache;
    cache.ttl_minutes = cfg.getPath("cache.ttl_minutes", cache.ttl_minutes);
    cache.userIndexPrefix = cfg.getPath("cache.userIndexPrefix", cache.userIndexPrefix);
    cache.detailPrefix = cfg.getPath("cache.detailPrefix", cache.detailPrefix);

    // --------------------------- 校验 ---------------------------
    if (!opt.validate()) {
        throw std::runtime_error("Invalid configuration detected");
    }

    return opt;
}

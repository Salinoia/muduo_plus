#include "OrderConfig.h"

#include <thread>

#include "ConfigLoader.hpp"

OrderServerOptions OrderServerOptions::FromConfig(const std::string& path) {
    ConfigLoader cfg(path);
    OrderServerOptions opt;

    // --------------------------- 基础配置 ---------------------------
    opt.serviceName = cfg.get("serviceName", opt.serviceName);
    opt.httpThreadNum = cfg.get("httpThreadNum", std::thread::hardware_concurrency());
    opt.enableTLS = cfg.get("enableTLS", opt.enableTLS);

    // --------------------------- MQ ---------------------------
    opt.mq.url = cfg.getPath("mq.url", opt.mq.url);
    opt.mq.orderQueue = cfg.getPath("mq.orderQueue", opt.mq.orderQueue);

    // --------------------------- Redis ---------------------------
    opt.redis.host = cfg.getPath("redis.host", opt.redis.host);
    opt.redis.port = cfg.getPath("redis.port", opt.redis.port);
    opt.redis.password = cfg.getPath("redis.password", opt.redis.password);
    opt.redis.poolSize = cfg.getPath("redis.poolSize", opt.redis.poolSize);
    opt.redis.timeoutMs = cfg.getPath("redis.timeoutMs", opt.redis.timeoutMs);
    opt.redis.keyPrefix = cfg.getPath("redis.keyPrefix", opt.redis.keyPrefix);

    // --------------------------- Database ---------------------------
    opt.database.connInfo.url = cfg.getPath<std::string>("database.connInfo.url", "");
    opt.database.connInfo.user = cfg.getPath<std::string>("database.connInfo.user", "");
    opt.database.connInfo.password = cfg.getPath<std::string>("database.connInfo.password", "");
    opt.database.connInfo.database = cfg.getPath<std::string>("database.connInfo.database", "");
    opt.database.connInfo.timeout_sec = cfg.getPath<int>("database.connInfo.timeout_sec", 5);
    // 验证配置
    if (!opt.validate()) {
        throw std::runtime_error("Invalid configuration detected");
    }
    return opt;
}

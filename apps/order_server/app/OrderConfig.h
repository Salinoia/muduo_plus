#pragma once
#include <string>

#include "MySQLConnInfo.h"

// --------------------------- MQ ---------------------------
struct MQOptions {
    std::string url;
    std::string orderQueue{"order.events"};

    bool validate() const { return !url.empty() && !orderQueue.empty(); }
};

// --------------------------- Redis ---------------------------
struct RedisOptions {
    std::string host{"127.0.0.1"};
    int port{6379};
    std::string password;
    std::size_t poolSize{4};
    int timeoutMs{1000};
    std::string keyPrefix{"order:"};

    bool validate() const { return !host.empty() && port > 0 && port < 65536; }
};

// --------------------------- Database ---------------------------
struct DatabaseOptions {
    MySQLConnInfo connInfo;

    bool validate() const { return connInfo.validate(); }
};

// --------------------------- OrderServer ---------------------------
struct OrderServerOptions {
    std::string serviceName{"OrderServer"};
    unsigned int httpThreadNum{0};
    bool enableTLS{false};

    MQOptions mq;
    RedisOptions redis;
    DatabaseOptions database;
    bool validate() const { return !serviceName.empty() && httpThreadNum >= 0 && mq.validate() && redis.validate() && database.validate(); }
    static OrderServerOptions FromConfig(const std::string& path);
};

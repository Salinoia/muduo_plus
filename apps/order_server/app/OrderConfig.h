#pragma once
#include <string>
#include "MySQLConnInfo.h"

// --------------------------- MQ ---------------------------
struct MQOptions {
    std::string url;
    std::string orderQueue{"order.events"};
    std::string inventoryQueue{"inventory.events"};
    std::string exchange{"order.exchange"};
    bool enableConsumer{true};

    bool validate() const { return !url.empty() && !orderQueue.empty() && !exchange.empty(); }
};

// --------------------------- Redis ---------------------------
struct RedisOptions {
    std::string host{"127.0.0.1"};
    int port{6379};
    std::string password;
    std::size_t poolSize{4};
    int timeoutMs{1000};
    std::string keyPrefix{"order:"};
    bool enableCache{true};

    bool validate() const { return !host.empty() && port > 0 && port < 65536; }
};

// --------------------------- Database ---------------------------
struct DatabaseOptions {
    MySQLConnInfo connInfo;
    int maxConnections{16};
    int minConnections{4};
    int maxIdleTime{60};
    int connectTimeout{5};

    bool validate() const { return connInfo.validate() && maxConnections >= minConnections; }
};

// --------------------------- Metrics ---------------------------
struct MetricsOptions {
    bool enablePrometheus{false};
    int port{9090};
};

// --------------------------- Logging ---------------------------
struct LoggingOptions {
    std::string level{"INFO"};
    bool console{true};
    std::string file{"./logs/order_server.log"};
};

// --------------------------- Reservation ---------------------------
struct ReservationOptions {
    int ttl_seconds{300};
    std::string restockRoutingKey{"inventory.restock"};
    std::string reservationRoutingKey{"inventory.reservation"};
};

// --------------------------- Cache ---------------------------
struct CacheOptions {
    int ttl_minutes{10};
    std::string userIndexPrefix{"user_orders:"};
    std::string detailPrefix{"order:"};
};

// --------------------------- OrderServer ---------------------------
struct OrderServerOptions {
    std::string serviceName{"OrderServer"};
    unsigned int httpThreadNum{0};
    bool enableTLS{false};

    MQOptions mq;
    RedisOptions redis;
    DatabaseOptions database;
    MetricsOptions metrics;
    LoggingOptions logging;
    ReservationOptions reservation;
    CacheOptions cache;

    bool validate() const { return !serviceName.empty() && httpThreadNum > 0 && mq.validate() && redis.validate() && database.validate(); }

    static OrderServerOptions FromConfig(const std::string& path);
};

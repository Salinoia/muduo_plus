#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "infra/db/OrderRepository.h"

class RedisPool;
class RedisClient;

class OrderCache {
public:
    using Record = OrderRepository::OrderRecord;
    using RecordList = OrderRepository::RecordList;
    using Clock = std::chrono::system_clock;

    struct Options {
        std::string keyPrefix{"order:"};
        std::string userIndexPrefix{"user_orders:"};
        std::chrono::seconds ttl{std::chrono::minutes(10)};
        bool enableUserIndex{true};
    };

    OrderCache(std::shared_ptr<RedisPool> pool, Options options);

    std::shared_ptr<RedisPool> pool() const noexcept { return pool_; }
    const Options& options() const noexcept { return options_; }

    bool PutOrder(const Record& record);
    bool PutOrders(const RecordList& records);
    std::optional<Record> GetOrder(const std::string& orderId);
    RecordList GetOrders(const std::vector<std::string>& orderIds);
    bool RemoveOrder(const std::string& orderId);
    bool RefreshTTL(const std::string& orderId, std::chrono::seconds ttl);

    bool PutUserOrders(const std::string& userId, const RecordList& records);
    std::optional<RecordList> GetUserOrders(const std::string& userId);
    bool RemoveUserOrders(const std::string& userId);

    void Warmup(const RecordList& records);
    void Clear();

private:
    std::string buildOrderKey(std::string_view orderId) const;
    std::string buildUserKey(std::string_view userId) const;

    std::string serializeOrder(const Record& record) const;
    Record deserializeOrder(const std::string& payload) const;
    std::string serializeOrderList(const RecordList& records) const;
    RecordList deserializeOrderList(const std::string& payload) const;

    bool setKey(RedisClient* client, const std::string& key, const std::string& value);
    std::optional<std::string> getKey(RedisClient* client, const std::string& key) const;
    bool deleteKey(RedisClient* client, const std::string& key);

private:
    std::shared_ptr<RedisPool> pool_;
    Options options_;
};

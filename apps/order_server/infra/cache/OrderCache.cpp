#include "infra/cache/OrderCache.h"

#include <sstream>
#include <utility>
#include <iomanip>

#include "infra/db/OrderRepository.h"
#include "RedisPool.h"

// ========== 构造函数 ==========

OrderCache::OrderCache(std::shared_ptr<RedisPool> pool, Options options) : pool_(std::move(pool)), options_(std::move(options)) {}

// ========== 核心接口 ==========

bool OrderCache::PutOrder(const Record& record) {
    if (!pool_)
        return false;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return false;

    std::string key = buildOrderKey(record.orderId);
    std::string value = serializeOrder(record);
    return setKey(client.get(), key, value);
}

bool OrderCache::PutOrders(const RecordList& records) {
    bool ok = true;
    for (const auto& r : records) {
        ok &= PutOrder(r);
    }
    return ok;
}

std::optional<OrderCache::Record> OrderCache::GetOrder(const std::string& orderId) {
    if (!pool_)
        return std::nullopt;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return std::nullopt;

    auto payload = getKey(client.get(), buildOrderKey(orderId));
    if (!payload)
        return std::nullopt;
    return deserializeOrder(*payload);
}

OrderCache::RecordList OrderCache::GetOrders(const std::vector<std::string>& orderIds) {
    RecordList result;
    result.reserve(orderIds.size());
    for (const auto& id : orderIds) {
        auto rec = GetOrder(id);
        if (rec)
            result.push_back(std::move(*rec));
    }
    return result;
}

bool OrderCache::RemoveOrder(const std::string& orderId) {
    if (!pool_)
        return false;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return false;
    return deleteKey(client.get(), buildOrderKey(orderId));
}

bool OrderCache::RefreshTTL(const std::string& orderId, std::chrono::seconds ttl) {
    if (!pool_)
        return false;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return false;

    // RedisClient 无显式 Expire 接口，这里重新 Set 相同值实现续期
    auto payload = getKey(client.get(), buildOrderKey(orderId));
    if (!payload)
        return false;
    return setKey(client.get(), buildOrderKey(orderId), *payload);
}

// ========== 用户订单索引 ==========

bool OrderCache::PutUserOrders(const std::string& userId, const RecordList& records) {
    if (!pool_ || !options_.enableUserIndex)
        return false;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return false;

    return setKey(client.get(), buildUserKey(userId), serializeOrderList(records));
}

std::optional<OrderCache::RecordList> OrderCache::GetUserOrders(const std::string& userId) {
    if (!pool_ || !options_.enableUserIndex)
        return std::nullopt;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return std::nullopt;

    auto payload = getKey(client.get(), buildUserKey(userId));
    if (!payload)
        return std::nullopt;
    return deserializeOrderList(*payload);
}

bool OrderCache::RemoveUserOrders(const std::string& userId) {
    if (!pool_ || !options_.enableUserIndex)
        return false;
    auto client = pool_->GetClient();
    if (!client || !client->IsConnected())
        return false;
    return deleteKey(client.get(), buildUserKey(userId));
}

// ========== 维护接口 ==========

void OrderCache::Warmup(const RecordList& records) {
    for (const auto& rec : records) {
        PutOrder(rec);
    }
}

void OrderCache::Clear() {
    // 无通配符删除接口，按约定只清理 prefix 范围由上层实现。
}

// ========== Key 工具函数 ==========

std::string OrderCache::buildOrderKey(std::string_view orderId) const {
    return options_.keyPrefix + std::string(orderId);
}

std::string OrderCache::buildUserKey(std::string_view userId) const {
    return options_.userIndexPrefix + std::string(userId);
}

// ========== 序列化 / 反序列化 ==========

std::string OrderCache::serializeOrder(const Record& record) const {
    std::ostringstream oss;
    oss << record.orderId << '|' << record.userId << '|' << record.productId << '|' << record.quantity << '|' << std::fixed << std::setprecision(2) << record.totalAmount << '|' << record.currency
        << '|' << static_cast<int>(record.status) << '|' << record.statusReason << '|' << record.payloadJson << '|'
        << std::chrono::duration_cast<std::chrono::seconds>(record.createdAt.time_since_epoch()).count() << '|'
        << std::chrono::duration_cast<std::chrono::seconds>(record.updatedAt.time_since_epoch()).count();
    return oss.str();
}

OrderCache::Record OrderCache::deserializeOrder(const std::string& payload) const {
    Record rec{};
    std::istringstream iss(payload);
    std::string token;

    std::getline(iss, rec.orderId, '|');
    std::getline(iss, rec.userId, '|');
    std::getline(iss, rec.productId, '|');

    std::getline(iss, token, '|');
    rec.quantity = static_cast<std::uint32_t>(std::stoul(token));

    std::getline(iss, token, '|');
    rec.totalAmount = std::stod(token);

    std::getline(iss, rec.currency, '|');

    std::getline(iss, token, '|');
    rec.status = static_cast<OrderStatus>(std::stoi(token));

    std::getline(iss, rec.statusReason, '|');
    std::getline(iss, rec.payloadJson, '|');

    std::getline(iss, token, '|');
    rec.createdAt = Clock::time_point{std::chrono::seconds(std::stoll(token))};

    std::getline(iss, token, '|');
    rec.updatedAt = Clock::time_point{std::chrono::seconds(std::stoll(token))};

    return rec;
}

std::string OrderCache::serializeOrderList(const RecordList& records) const {
    std::ostringstream oss;
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (i > 0)
            oss << '\n';
        oss << serializeOrder(records[i]);
    }
    return oss.str();
}

OrderCache::RecordList OrderCache::deserializeOrderList(const std::string& payload) const {
    RecordList list;
    std::istringstream iss(payload);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty())
            list.push_back(deserializeOrder(line));
    }
    return list;
}

// ========== Redis 基础封装 ==========

bool OrderCache::setKey(RedisClient* client, const std::string& key, const std::string& value) {
    return client && client->Set(key, value);
}

std::optional<std::string> OrderCache::getKey(RedisClient* client, const std::string& key) const {
    if (!client)
        return std::nullopt;
    std::string val;
    if (!client->Get(key, val))
        return std::nullopt;
    return val;
}

bool OrderCache::deleteKey(RedisClient* client, const std::string& key) {
    return client && client->Del(key);
}

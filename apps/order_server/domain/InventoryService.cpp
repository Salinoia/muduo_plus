#include "domain/InventoryService.h"

#include <sstream>
#include <utility>
#include <chrono>

#include "MQProducer.h"
#include "RedisPool.h"

InventoryService::InventoryService(Dependencies deps, Options options) : deps_(std::move(deps)), options_(std::move(options)) {}

// ========== 关键逻辑入口 ==========

bool InventoryService::ReserveForOrder(const Record& order, Reservation* outReservation, std::string* errorMessage) {
    if (!deps_.redis || !deps_.orders) {
        if (errorMessage)
            *errorMessage = "Missing dependencies";
        return false;
    }

    auto client = deps_.redis->GetClient();
    if (!client || !client->IsConnected()) {
        if (errorMessage)
            *errorMessage = "Failed to acquire Redis client";
        return false;
    }

    const std::string productKey = makeStockKey(order.productId);
    const std::string reservationId = makeReservationId(order);
    const std::string reservationKey = makeReservationKey(reservationId);

    std::string stockStr;
    if (!client->Get(productKey, stockStr)) {
        if (errorMessage)
            *errorMessage = "Failed to read stock from Redis";
        return false;
    }

    std::uint64_t stockValue = 0;
    try {
        stockValue = std::stoull(stockStr);
    } catch (...) {
        if (errorMessage)
            *errorMessage = "Invalid stock value format";
        return false;
    }

    if (stockValue < order.quantity) {
        if (errorMessage)
            *errorMessage = "Insufficient stock";
        return false;
    }

    // 扣减库存
    std::uint64_t newStock = stockValue - order.quantity;
    if (!client->Set(productKey, std::to_string(newStock))) {
        if (errorMessage)
            *errorMessage = "Failed to update stock";
        return false;
    }

    // 缓存预留记录
    Reservation reservation;
    reservation.reservationId = reservationId;
    reservation.orderId = order.orderId;
    reservation.productId = order.productId;
    reservation.quantity = order.quantity;
    reservation.expiresAt = Clock::now() + options_.reservationTTL;

    if (!cacheReservation(client.get(), reservation)) {
        if (errorMessage)
            *errorMessage = "Failed to cache reservation";
        return false;
    }

    if (options_.publishEvents && deps_.producer) {
        PublishReservationEvent(reservation, "created");
    }

    if (outReservation)
        *outReservation = reservation;
    return true;
}

bool InventoryService::CommitReservation(const Reservation& reservation, std::string* errorMessage) {
    if (!deps_.redis) {
        if (errorMessage)
            *errorMessage = "Missing Redis dependency";
        return false;
    }

    auto client = deps_.redis->GetClient();
    if (!client || !client->IsConnected()) {
        if (errorMessage)
            *errorMessage = "Failed to acquire Redis client";
        return false;
    }

    if (!deleteReservation(client.get(), reservation.reservationId)) {
        if (errorMessage)
            *errorMessage = "Failed to delete reservation cache";
        return false;
    }

    if (options_.publishEvents && deps_.producer) {
        PublishReservationEvent(reservation, "committed");
    }
    return true;
}

bool InventoryService::ReleaseReservation(const Reservation& reservation, std::string_view reason, std::string* errorMessage) {
    if (!deps_.redis) {
        if (errorMessage)
            *errorMessage = "Missing Redis dependency";
        return false;
    }

    auto client = deps_.redis->GetClient();
    if (!client || !client->IsConnected()) {
        if (errorMessage)
            *errorMessage = "Failed to acquire Redis client";
        return false;
    }

    // 恢复库存
    if (!incrementStock(client.get(), reservation.productId, reservation.quantity)) {
        if (errorMessage)
            *errorMessage = "Failed to restore stock";
        return false;
    }

    deleteReservation(client.get(), reservation.reservationId);

    if (options_.publishEvents && deps_.producer) {
        PublishReservationEvent(reservation, std::string("released:") + std::string(reason));
    }
    return true;
}

// ========== 库存操作 ==========

bool InventoryService::AdjustStock(const std::string& productId, std::int64_t delta) {
    if (!deps_.redis)
        return false;

    auto client = deps_.redis->GetClient();
    if (!client || !client->IsConnected())
        return false;

    std::string key = makeStockKey(productId);
    std::string value;
    if (!client->Get(key, value))
        return false;

    std::int64_t stock = 0;
    try {
        stock = std::stoll(value);
    } catch (...) {
        return false;
    }

    stock += delta;
    if (stock < 0)
        stock = 0;
    return client->Set(key, std::to_string(stock));
}

bool InventoryService::SetStock(const std::string& productId, std::uint64_t amount) {
    if (!deps_.redis)
        return false;
    auto client = deps_.redis->GetClient();
    if (!client || !client->IsConnected())
        return false;
    return client->Set(makeStockKey(productId), std::to_string(amount));
}

std::optional<std::uint64_t> InventoryService::QueryStock(const std::string& productId) const {
    if (!deps_.redis)
        return std::nullopt;
    auto client = deps_.redis->GetClient();
    if (!client || !client->IsConnected())
        return std::nullopt;

    std::string value;
    if (!client->Get(makeStockKey(productId), value))
        return std::nullopt;

    try {
        return std::stoull(value);
    } catch (...) {
        return std::nullopt;
    }
}

bool InventoryService::SyncStockFromDatabase(const std::string& productId) {
    if (!deps_.redis || !deps_.orders)
        return false;
    // TODO: 从数据库计算总库存同步到 Redis（暂留）
    return true;
}

// ========== 事件发布 ==========

void InventoryService::PublishReservationEvent(const Reservation& reservation, std::string_view eventType) {
    if (!deps_.producer)
        return;

    std::ostringstream oss;
    oss << "{"
        << "\"reservationId\":\"" << reservation.reservationId << "\","
        << "\"orderId\":\"" << reservation.orderId << "\","
        << "\"productId\":\"" << reservation.productId << "\","
        << "\"quantity\":" << reservation.quantity << ","
        << "\"eventType\":\"" << eventType << "\""
        << "}";

    deps_.producer->publish(options_.eventExchange, options_.reservationRoutingKey, oss.str());
}

void InventoryService::PublishRestockEvent(const std::string& productId, std::int64_t quantity) {
    if (!deps_.producer)
        return;

    std::ostringstream oss;
    oss << "{"
        << "\"productId\":\"" << productId << "\","
        << "\"quantity\":" << quantity << ","
        << "\"eventType\":\"restock\""
        << "}";

    deps_.producer->publish(options_.eventExchange, options_.restockRoutingKey, oss.str());
}

// ========== Redis Key 生成工具 ==========

std::string InventoryService::makeStockKey(std::string_view productId) const {
    return options_.stockKeyPrefix + std::string(productId);
}

std::string InventoryService::makeReservationKey(std::string_view reservationId) const {
    return options_.reservationKeyPrefix + std::string(reservationId);
}

std::string InventoryService::makeReservationId(const Record& order) const {
    return order.orderId + ":" + order.productId;
}

// ========== Redis 内部操作 ==========

bool InventoryService::decrementStock(RedisClient* client, const std::string& productId, std::uint32_t quantity) {
    std::string key = makeStockKey(productId);
    std::string val;
    if (!client->Get(key, val))
        return false;

    std::uint64_t stock = 0;
    try {
        stock = std::stoull(val);
    } catch (...) {
        return false;
    }

    if (stock < quantity)
        return false;
    stock -= quantity;
    return client->Set(key, std::to_string(stock));
}

bool InventoryService::incrementStock(RedisClient* client, const std::string& productId, std::uint32_t quantity) {
    std::string key = makeStockKey(productId);
    std::string val;
    if (!client->Get(key, val))
        return false;

    std::uint64_t stock = 0;
    try {
        stock = std::stoull(val);
    } catch (...) {
        return false;
    }

    stock += quantity;
    return client->Set(key, std::to_string(stock));
}

bool InventoryService::cacheReservation(RedisClient* client, const Reservation& reservation) {
    std::ostringstream oss;
    oss << reservation.orderId << "," << reservation.productId << "," << reservation.quantity << ",";
    auto key = makeReservationKey(reservation.reservationId);
    return client->Set(key, oss.str());
}

std::optional<InventoryService::Reservation> InventoryService::fetchReservation(RedisClient* client, const std::string& reservationId) const {
    std::string value;
    if (!client->Get(makeReservationKey(reservationId), value))
        return std::nullopt;

    std::istringstream iss(value);
    Reservation r;
    r.reservationId = reservationId;
    std::getline(iss, r.orderId, ',');
    std::getline(iss, r.productId, ',');
    iss >> r.quantity;
    r.expiresAt = Clock::now() + options_.reservationTTL;
    return r;
}

bool InventoryService::deleteReservation(RedisClient* client, const std::string& reservationId) {
    return client->Del(makeReservationKey(reservationId));
}

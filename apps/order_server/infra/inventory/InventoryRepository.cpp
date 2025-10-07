#include "infra/inventory/InventoryRepository.h"

#include <sstream>
#include <utility>

#include "RedisPool.h"
#include "RedisClient.h"

// ========== 构造函数 ==========

InventoryRepository::InventoryRepository(std::shared_ptr<RedisPool> pool) : InventoryRepository(std::move(pool), Options{}) {}

InventoryRepository::InventoryRepository(std::shared_ptr<RedisPool> pool, Options options) : pool_(std::move(pool)), options_(std::move(options)) {}

// ========== 库存操作 ==========

bool InventoryRepository::DecrementStock(const std::string& productId, std::uint32_t quantity) {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return false;

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

bool InventoryRepository::IncrementStock(const std::string& productId, std::uint32_t quantity) {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return false;

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

bool InventoryRepository::SetStock(const std::string& productId, std::uint64_t amount) {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return false;

    return client->Set(makeStockKey(productId), std::to_string(amount));
}

std::optional<std::uint64_t> InventoryRepository::QueryStock(const std::string& productId) const {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return std::nullopt;

    std::string val;
    if (!client->Get(makeStockKey(productId), val))
        return std::nullopt;

    try {
        return std::stoull(val);
    } catch (...) {
        return std::nullopt;
    }
}

// ========== 预留操作 ==========

bool InventoryRepository::SaveReservation(const ReservationRecord& reservation) {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return false;

    std::ostringstream oss;
    oss << reservation.orderId << '|' << reservation.productId << '|' << reservation.quantity << '|'
        << std::chrono::duration_cast<std::chrono::seconds>(reservation.expiresAt.time_since_epoch()).count();

    return client->Set(makeReservationKey(reservation.reservationId), oss.str());
}

std::optional<InventoryRepository::ReservationRecord> InventoryRepository::GetReservation(const std::string& reservationId) const {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return std::nullopt;

    std::string val;
    if (!client->Get(makeReservationKey(reservationId), val))
        return std::nullopt;

    std::istringstream iss(val);
    std::string token;
    ReservationRecord record;
    record.reservationId = reservationId;

    std::getline(iss, record.orderId, '|');
    std::getline(iss, record.productId, '|');

    std::getline(iss, token, '|');
    record.quantity = static_cast<std::uint32_t>(std::stoul(token));

    std::getline(iss, token, '|');
    record.expiresAt = Clock::time_point{std::chrono::seconds(std::stoll(token))};

    return record;
}

bool InventoryRepository::DeleteReservation(const std::string& reservationId) {
    auto client = borrowClient();
    if (!client || !client->IsConnected())
        return false;
    return client->Del(makeReservationKey(reservationId));
}

// ========== 工具函数 ==========

std::string InventoryRepository::makeStockKey(std::string_view productId) const {
    return options_.stockKeyPrefix + std::string(productId);
}

std::string InventoryRepository::makeReservationKey(std::string_view reservationId) const {
    return options_.reservationKeyPrefix + std::string(reservationId);
}

std::shared_ptr<RedisClient> InventoryRepository::borrowClient() const {
    if (!pool_)
        return nullptr;
    return pool_->GetClient();
}

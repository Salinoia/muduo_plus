#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class RedisPool;
class RedisClient;

/**
 * @brief InventoryRepository：库存数据访问层
 *
 * 对 Redis 缓存中的库存与预留记录进行统一读写封装，隔离底层实现细节。
 */
class InventoryRepository {
public:
    using Clock = std::chrono::system_clock;

    struct Options {
        std::string stockKeyPrefix{"inventory:stock:"};
        std::string reservationKeyPrefix{"inventory:reservation:"};
        std::chrono::seconds reservationTTL{std::chrono::minutes(5)};
    };

    struct ReservationRecord {
        std::string reservationId;
        std::string orderId;
        std::string productId;
        std::uint32_t quantity{0};
        Clock::time_point expiresAt{};
    };

    explicit InventoryRepository(std::shared_ptr<RedisPool> pool);
    InventoryRepository(std::shared_ptr<RedisPool> pool, Options options);

    std::shared_ptr<RedisPool> pool() const noexcept { return pool_; }
    const Options& options() const noexcept { return options_; }

    bool DecrementStock(const std::string& productId, std::uint32_t quantity);
    bool IncrementStock(const std::string& productId, std::uint32_t quantity);
    bool SetStock(const std::string& productId, std::uint64_t amount);
    std::optional<std::uint64_t> QueryStock(const std::string& productId) const;

    bool SaveReservation(const ReservationRecord& reservation);
    std::optional<ReservationRecord> GetReservation(const std::string& reservationId) const;
    bool DeleteReservation(const std::string& reservationId);

private:
    std::string makeStockKey(std::string_view productId) const;
    std::string makeReservationKey(std::string_view reservationId) const;
    std::shared_ptr<RedisClient> borrowClient() const;

private:
    std::shared_ptr<RedisPool> pool_;
    Options options_;
};

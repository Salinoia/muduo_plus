#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "infra/db/OrderRepository.h"

class RedisPool;
class RedisClient;
class MQProducer;

class InventoryService {
public:
    using Clock = std::chrono::system_clock;
    using Record = OrderRepository::OrderRecord;
    struct Dependencies {
        RedisPool* redis{nullptr};
        MQProducer* producer{nullptr};
        OrderRepository* orders{nullptr};
    };

    struct Options {
        std::string stockKeyPrefix{"inventory:stock:"};
        std::string reservationKeyPrefix{"inventory:reservation:"};
        std::chrono::seconds reservationTTL{std::chrono::minutes(5)};
        bool publishEvents{true};
        std::string eventExchange{};
        std::string reservationRoutingKey{"inventory.reservation"};
        std::string restockRoutingKey{"inventory.restock"};
    };

    struct Reservation {
        std::string reservationId;
        std::string orderId;
        std::string productId;
        std::uint32_t quantity{0};
        Clock::time_point expiresAt{};
    };

    InventoryService(Dependencies deps, Options options);

    bool ReserveForOrder(const Record& order, Reservation* outReservation, std::string* errorMessage = nullptr);
    bool CommitReservation(const Reservation& reservation, std::string* errorMessage = nullptr);
    bool ReleaseReservation(const Reservation& reservation, std::string_view reason, std::string* errorMessage = nullptr);

    bool AdjustStock(const std::string& productId, std::int64_t delta);
    bool SetStock(const std::string& productId, std::uint64_t amount);
    std::optional<std::uint64_t> QueryStock(const std::string& productId) const;
    bool SyncStockFromDatabase(const std::string& productId);

    void PublishReservationEvent(const Reservation& reservation, std::string_view eventType);
    void PublishRestockEvent(const std::string& productId, std::int64_t quantity);

private:
    std::string makeStockKey(std::string_view productId) const;
    std::string makeReservationKey(std::string_view reservationId) const;
    std::string makeReservationId(const Record& order) const;

    bool decrementStock(RedisClient* client, const std::string& productId, std::uint32_t quantity);
    bool incrementStock(RedisClient* client, const std::string& productId, std::uint32_t quantity);
    bool cacheReservation(RedisClient* client, const Reservation& reservation);
    std::optional<Reservation> fetchReservation(RedisClient* client, const std::string& reservationId) const;
    bool deleteReservation(RedisClient* client, const std::string& reservationId);

    Dependencies deps_;
    Options options_;
};

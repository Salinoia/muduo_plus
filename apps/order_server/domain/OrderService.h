#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "domain/InventoryService.h"
#include "domain/OrderEntity.h"

class OrderCache;
class OrderRepository;
class MQProducer;

/**
 * @brief OrderService：订单领域服务
 *
 * 聚合订单、库存、缓存、消息等依赖，对外提供统一的业务服务接口。
 */
class OrderService {
public:
    using Clock = OrderEntity::Clock;
    using Entity = OrderEntity;
    using EntityList = std::vector<Entity>;
    using Reservation = InventoryService::Reservation;

    struct Dependencies {
        OrderRepository* database{nullptr};
        OrderCache* cache{nullptr};
        InventoryService* inventory{nullptr};
        MQProducer* producer{nullptr};
    };

    struct Options {
        bool useCache{true};
        bool useMessageQueue{true};
        bool requireInventoryReservation{true};
        std::size_t defaultPageSize{20};
        std::size_t maxPageSize{100};
    };

    struct CreateContext {
        Entity entity;
        std::string rawPayload;
        bool skipReservation{false};
    };

    struct CreateResult {
        Entity entity;
        std::optional<Reservation> reservation;
    };

    OrderService(Dependencies deps);
    OrderService(Dependencies deps, Options options);

    const Dependencies& deps() const noexcept { return deps_; }
    const Options& options() const noexcept { return options_; }

    std::optional<Entity> GetOrderById(const std::string& orderId, bool preferCache = true) const;
    EntityList ListOrdersByUser(const std::string& userId, std::size_t limit, std::size_t offset, bool preferCache = true) const;

    std::optional<CreateResult> CreateOrder(CreateContext ctx, std::string* errorMessage = nullptr);

    bool UpdateStatus(const std::string& orderId, OrderStatus status, const std::string& reason = {});
    bool MarkPaid(const std::string& orderId, double paidAmount, Clock::time_point paidAt);
    bool CancelOrder(const std::string& orderId, const std::string& reason, bool releaseReservation = true);

    void WarmupCache(const EntityList& entities) const;
    void RefreshCache(const std::string& orderId) const;

private:
    Entity hydrate(const OrderRepository::OrderRecord& record) const;
    OrderRepository::OrderRecord dehydrate(const Entity& entity) const;
    std::optional<Entity> fetchFromCache(const std::string& orderId) const;
    std::optional<Entity> fetchFromDatabase(const std::string& orderId) const;
    void publishEvent(const std::string& event, const Entity& entity, std::string_view payload = {}) const;

private:
    Dependencies deps_;
    Options options_;
};

#include "domain/OrderService.h"

#include <sstream>
#include <utility>
#include <chrono>

#include "infra/db/OrderRepository.h"
#include "infra/cache/OrderCache.h"
#include "MQProducer.h"

// ========== 构造函数 ==========
OrderService::OrderService(Dependencies deps) : OrderService(std::move(deps), Options{}) {}

OrderService::OrderService(Dependencies deps, Options options) : deps_(std::move(deps)), options_(std::move(options)) {}

// ========== 查询接口 ==========

std::optional<OrderService::Entity> OrderService::GetOrderById(const std::string& orderId, bool preferCache) const {
    if (preferCache && options_.useCache && deps_.cache) {
        if (auto cached = fetchFromCache(orderId))
            return cached;
    }
    auto db = fetchFromDatabase(orderId);
    if (db && options_.useCache && deps_.cache) {
        deps_.cache->PutOrder(dehydrate(*db));
    }
    return db;
}

OrderService::EntityList OrderService::ListOrdersByUser(const std::string& userId, std::size_t limit, std::size_t offset, bool preferCache) const {
    EntityList result;
    if (limit == 0)
        return result;
    limit = std::min(limit, options_.maxPageSize);

    if (preferCache && options_.useCache && deps_.cache) {
        auto cached = deps_.cache->GetUserOrders(userId);
        if (cached) {
            result.reserve(cached->size());
            for (const auto& rec : *cached)
                result.push_back(hydrate(rec));
            return result;
        }
    }

    if (!deps_.database)
        return result;
    auto records = deps_.database->ListByUser(userId, limit, offset);
    result.reserve(records.size());
    for (const auto& rec : records)
        result.push_back(hydrate(rec));

    if (options_.useCache && deps_.cache) {
        deps_.cache->PutUserOrders(userId, records);
    }
    return result;
}

// ========== 创建接口 ==========

std::optional<OrderService::CreateResult> OrderService::CreateOrder(CreateContext ctx, std::string* errorMessage) {
    if (!deps_.database) {
        if (errorMessage)
            *errorMessage = "Missing database dependency";
        return std::nullopt;
    }

    Entity entity = std::move(ctx.entity);
    entity.SetPayload(ctx.rawPayload);
    entity.SetCreatedAt(Clock::now());
    entity.MarkPending("order created");

    std::optional<Reservation> reservation;
    if (options_.requireInventoryReservation && deps_.inventory && !ctx.skipReservation) {
        InventoryService::Reservation r;
        if (!deps_.inventory->ReserveForOrder(entity.ToRecord(), &r, errorMessage)) {
            entity.MarkFailed("inventory reservation failed");
            deps_.database->Insert(entity.ToRecord());
            if (options_.useCache && deps_.cache)
                deps_.cache->PutOrder(entity.ToRecord());
            return std::nullopt;
        }
        reservation = r;
    }

    if (!deps_.database->Insert(entity.ToRecord())) {
        if (errorMessage)
            *errorMessage = "Failed to insert order record";
        if (reservation && deps_.inventory) {
            deps_.inventory->ReleaseReservation(*reservation, "rollback");
        }
        return std::nullopt;
    }

    if (options_.useCache && deps_.cache)
        deps_.cache->PutOrder(entity.ToRecord());

    if (options_.useMessageQueue && deps_.producer)
        publishEvent("order.created", entity, ctx.rawPayload);

    return CreateResult{std::move(entity), reservation};
}

// ========== 状态更新接口 ==========

bool OrderService::UpdateStatus(const std::string& orderId, OrderStatus status, const std::string& reason) {
    if (!deps_.database)
        return false;
    if (!deps_.database->UpdateStatus(orderId, status, reason))
        return false;

    if (options_.useCache && deps_.cache)
        RefreshCache(orderId);

    if (options_.useMessageQueue && deps_.producer) {
        Entity entity;
        if (auto e = GetOrderById(orderId))
            entity = *e;
        publishEvent("order.status_updated", entity, reason);
    }
    return true;
}

bool OrderService::MarkPaid(const std::string& orderId, double paidAmount, Clock::time_point paidAt) {
    if (!deps_.database)
        return false;
    if (!deps_.database->UpdatePayment(orderId, paidAmount, paidAt))
        return false;

    if (options_.useCache && deps_.cache)
        RefreshCache(orderId);

    if (options_.useMessageQueue && deps_.producer) {
        if (auto e = GetOrderById(orderId))
            publishEvent("order.paid", *e);
    }
    return true;
}

bool OrderService::CancelOrder(const std::string& orderId, const std::string& reason, bool releaseReservation) {
    if (!deps_.database)
        return false;
    auto record = deps_.database->GetById(orderId);
    if (!record)
        return false;

    if (!deps_.database->UpdateStatus(orderId, OrderStatus::kCancelled, reason))
        return false;

    if (releaseReservation && deps_.inventory) {
        InventoryService::Reservation r;
        r.orderId = record->orderId;
        r.productId = record->productId;
        r.quantity = record->quantity;
        deps_.inventory->ReleaseReservation(r, "order cancelled");
    }

    if (options_.useCache && deps_.cache)
        RefreshCache(orderId);

    if (options_.useMessageQueue && deps_.producer) {
        publishEvent("order.cancelled", hydrate(*record), reason);
    }
    return true;
}

// ========== 缓存维护接口 ==========

void OrderService::WarmupCache(const EntityList& entities) const {
    if (!deps_.cache || !options_.useCache)
        return;
    OrderRepository::RecordList list;
    list.reserve(entities.size());
    for (const auto& e : entities)
        list.push_back(e.ToRecord());
    deps_.cache->PutOrders(list);
}

void OrderService::RefreshCache(const std::string& orderId) const {
    if (!deps_.cache || !options_.useCache)
        return;
    auto record = deps_.database->GetById(orderId);
    if (record)
        deps_.cache->PutOrder(*record);
}

// ========== 内部工具函数 ==========

OrderService::Entity OrderService::hydrate(const OrderRepository::OrderRecord& record) const {
    return Entity::FromRecord(record);
}

OrderRepository::OrderRecord OrderService::dehydrate(const Entity& entity) const {
    return entity.ToRecord();
}

std::optional<OrderService::Entity> OrderService::fetchFromCache(const std::string& orderId) const {
    if (!deps_.cache)
        return std::nullopt;
    auto rec = deps_.cache->GetOrder(orderId);
    if (!rec)
        return std::nullopt;
    return hydrate(*rec);
}

std::optional<OrderService::Entity> OrderService::fetchFromDatabase(const std::string& orderId) const {
    if (!deps_.database)
        return std::nullopt;
    auto rec = deps_.database->GetById(orderId);
    if (!rec)
        return std::nullopt;
    return hydrate(*rec);
}

void OrderService::publishEvent(const std::string& event, const Entity& entity, std::string_view payload) const {
    if (!deps_.producer)
        return;

    std::ostringstream oss;
    oss << "{"
        << "\"event\":\"" << event << "\","
        << "\"orderId\":\"" << entity.id() << "\","
        << "\"userId\":\"" << entity.userId() << "\","
        << "\"productId\":\"" << entity.productId() << "\","
        << "\"status\":\"" << static_cast<int>(entity.status()) << "\"";
    if (!payload.empty())
        oss << ",\"payload\":\"" << payload << "\"";
    oss << "}";

    deps_.producer->publish("", "order.events", oss.str());
}

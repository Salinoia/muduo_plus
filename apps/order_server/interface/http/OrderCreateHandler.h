#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "infra/db/OrderRepository.h"
#include "RouterHandler.h"
#include "HttpResponse.h"

class HttpRequest;
class OrderCache;
class MQProducer;
class InventoryService;

class OrderCreateHandler : public RouterHandler {
public:
    using Clock = std::chrono::system_clock;
    using IdGenerator = std::function<std::string()>;

    struct Dependencies {
        OrderRepository* database{nullptr};
        OrderCache* cache{nullptr};
        InventoryService* inventory{nullptr};
        MQProducer* producer{nullptr};
    };

    struct Options {
        std::string mqExchange{};
        std::string mqRoutingKey{"order.events"};
        bool enableCache{true};
        bool enableMqPublish{true};
        bool requireInventoryReservation{true};
    };

    OrderCreateHandler(Dependencies deps, Options options);

    void handle(const HttpRequest& req, HttpResponse* resp) override;

    void setIdGenerator(IdGenerator generator);

private:
    bool ensureDependencies(HttpResponse* resp) const;
    bool parseRequest(const HttpRequest& req, OrderRepository::OrderRecord* outRecord, std::string* rawPayload, HttpResponse* resp) const;
    bool validateRecord(const OrderRepository::OrderRecord& record, HttpResponse* resp) const;
    bool reserveInventory(const OrderRepository::OrderRecord& record, HttpResponse* resp);
    bool persistOrder(OrderRepository::OrderRecord& record, const std::string& rawPayload, HttpResponse* resp);
    void updateCache(const OrderRepository::OrderRecord& record, const std::string& rawPayload);
    void publishOrderEvent(const OrderRepository::OrderRecord& record, const std::string& rawPayload);
    void respondSuccess(const OrderRepository::OrderRecord& record, HttpResponse* resp) const;
    void respondError(HttpResponse* resp, HttpResponse::HttpStatusCode code, std::string_view message) const;

    Dependencies deps_;
    Options options_;
    IdGenerator idGenerator_;
};

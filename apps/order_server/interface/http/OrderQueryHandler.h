#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "infra/db/OrderRepository.h"
#include "RouterHandler.h"


class HttpRequest;
class HttpResponse;
class OrderCache;

class OrderQueryHandler : public RouterHandler {
public:
    using Record = OrderRepository::OrderRecord;
    using RecordList = OrderRepository::RecordList;

    struct Dependencies {
        OrderRepository* database{nullptr};
        OrderCache* cache{nullptr};
    };

    struct Options {
        bool preferCache{true};
        std::size_t maxPageSize{100};
        std::chrono::seconds cacheWarmupTTL{std::chrono::minutes(10)};
    };

    OrderQueryHandler(Dependencies deps, Options options);

    void handle(const HttpRequest& req, HttpResponse* resp) override;

private:
    bool ensureDependencies(HttpResponse* resp) const;

    bool handleGetById(const HttpRequest& req, HttpResponse* resp);
    bool handleListByUser(const HttpRequest& req, HttpResponse* resp);

    std::optional<std::string> extractOrderId(const HttpRequest& req) const;
    std::optional<std::string> extractUserId(const HttpRequest& req) const;
    std::size_t extractLimit(const HttpRequest& req) const;
    std::size_t extractOffset(const HttpRequest& req) const;

    bool respondRecord(const Record& record, HttpResponse* resp) const;
    bool respondRecords(const RecordList& records, HttpResponse* resp) const;
    void respondNotFound(HttpResponse* resp) const;
    void respondBadRequest(HttpResponse* resp, std::string_view message) const;
    void respondServerError(HttpResponse* resp, std::string_view message) const;

    std::optional<Record> fetchFromCache(const std::string& orderId) const;
    std::optional<Record> fetchFromDatabase(const std::string& orderId) const;
    RecordList fetchListFromCache(const std::string& userId) const;
    RecordList fetchListFromDatabase(const std::string& userId, std::size_t limit, std::size_t offset) const;

    void warmupOrderCache(const Record& record) const;
    void warmupUserCache(const std::string& userId, const RecordList& records) const;

    Dependencies deps_;
    Options options_;
};

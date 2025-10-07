#include "interface/http/OrderQueryHandler.h"

#include <nlohmann/json.hpp>

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "LogMacros.h"

#include "infra/db/OrderRepository.h"
#include "infra/cache/OrderCache.h"

using json = nlohmann::json;

OrderQueryHandler::OrderQueryHandler(Dependencies deps, Options options) : deps_(std::move(deps)), options_(std::move(options)) {}

/**
 * 主入口：根据 query 参数分流
 */
void OrderQueryHandler::handle(const HttpRequest& req, HttpResponse* resp) {
    LOG_INFO("Incoming order.query request: {}", req.query());

    if (!ensureDependencies(resp))
        return;

    // 优先处理单条查询
    if (auto orderId = extractOrderId(req)) {
        if (!handleGetById(req, resp))
            LOG_WARN("OrderQueryHandler handleGetById failed for {}", *orderId);
        return;
    }

    // 再处理列表查询
    if (auto userId = extractUserId(req)) {
        if (!handleListByUser(req, resp))
            LOG_WARN("OrderQueryHandler handleListByUser failed for {}", *userId);
        return;
    }

    // 都不匹配
    respondBadRequest(resp, "Missing query parameter: id or userId");
}

/**
 * ================== 内部逻辑 ==================
 */
bool OrderQueryHandler::ensureDependencies(HttpResponse* resp) const {
    if (!deps_.database) {
        respondServerError(resp, "Database dependency missing");
        LOG_ERROR("OrderQueryHandler missing OrderRepository dependency");
        return false;
    }
    return true;
}

bool OrderQueryHandler::handleGetById(const HttpRequest& req, HttpResponse* resp) {
    auto orderIdOpt = extractOrderId(req);
    if (!orderIdOpt) {
        respondBadRequest(resp, "Missing order id");
        return false;
    }
    const auto& orderId = *orderIdOpt;

    std::optional<Record> record;

    if (options_.preferCache && deps_.cache) {
        record = fetchFromCache(orderId);
        if (record) {
            LOG_DEBUG("Cache hit for orderId={}", orderId);
            return respondRecord(*record, resp);
        }
        LOG_INFO("Cache miss for orderId={}, fallback to DB", orderId);
    }

    record = fetchFromDatabase(orderId);
    if (!record) {
        respondNotFound(resp);
        return false;
    }

    // DB 查询成功，warmup 缓存
    warmupOrderCache(*record);
    return respondRecord(*record, resp);
}

bool OrderQueryHandler::handleListByUser(const HttpRequest& req, HttpResponse* resp) {
    auto userIdOpt = extractUserId(req);
    if (!userIdOpt) {
        respondBadRequest(resp, "Missing userId");
        return false;
    }

    const auto& userId = *userIdOpt;
    std::size_t limit = extractLimit(req);
    std::size_t offset = extractOffset(req);

    RecordList records;

    if (options_.preferCache && deps_.cache) {
        records = fetchListFromCache(userId);
        if (!records.empty()) {
            LOG_DEBUG("Cache hit for userId={}, size={}", userId, records.size());
            return respondRecords(records, resp);
        }
        LOG_INFO("Cache miss for userId={}, fallback to DB", userId);
    }

    records = fetchListFromDatabase(userId, limit, offset);
    if (records.empty()) {
        respondNotFound(resp);
        return false;
    }

    warmupUserCache(userId, records);
    return respondRecords(records, resp);
}

/**
 * ================== 参数提取 ==================
 */
std::optional<std::string> OrderQueryHandler::extractOrderId(const HttpRequest& req) const {
    std::string id = req.getQueryParameter("id");
    if (id.empty())
        return std::nullopt;
    return id;
}

std::optional<std::string> OrderQueryHandler::extractUserId(const HttpRequest& req) const {
    std::string uid = req.getQueryParameter("userId");
    if (uid.empty())
        return std::nullopt;
    return uid;
}

std::size_t OrderQueryHandler::extractLimit(const HttpRequest& req) const {
    std::string l = req.getQueryParameter("limit");
    std::size_t limit = 20;
    try {
        if (!l.empty())
            limit = std::stoul(l);
    } catch (...) {
        LOG_WARN("Invalid limit param: {}", l);
    }
    return std::min(limit, options_.maxPageSize);
}

std::size_t OrderQueryHandler::extractOffset(const HttpRequest& req) const {
    std::string o = req.getQueryParameter("offset");
    std::size_t offset = 0;
    try {
        if (!o.empty())
            offset = std::stoul(o);
    } catch (...) {
        LOG_WARN("Invalid offset param: {}", o);
    }
    return offset;
}

/**
 * ================== 响应序列化 ==================
 */
bool OrderQueryHandler::respondRecord(const Record& record, HttpResponse* resp) const {
    json j;
    j["orderId"] = record.orderId;
    j["userId"] = record.userId;
    j["productId"] = record.productId;
    j["quantity"] = record.quantity;
    j["totalAmount"] = record.totalAmount;
    j["currency"] = record.currency;
    j["status"] = ToString(record.status);
    j["statusReason"] = record.statusReason;
    j["createdAt"] = std::chrono::duration_cast<std::chrono::seconds>(record.createdAt.time_since_epoch()).count();
    j["updatedAt"] = std::chrono::duration_cast<std::chrono::seconds>(record.updatedAt.time_since_epoch()).count();

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(j.dump());
    return true;
}

bool OrderQueryHandler::respondRecords(const RecordList& records, HttpResponse* resp) const {
    json arr = json::array();
    for (const auto& r : records) {
        json j;
        j["orderId"] = r.orderId;
        j["productId"] = r.productId;
        j["quantity"] = r.quantity;
        j["totalAmount"] = r.totalAmount;
        j["status"] = ToString(r.status);
        arr.push_back(j);
    }

    json result;
    result["total"] = records.size();
    result["orders"] = arr;

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(result.dump());
    return true;
}

void OrderQueryHandler::respondNotFound(HttpResponse* resp) const {
    json j;
    j["error"] = "Record not found";
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setContentType("application/json");
    resp->setBody(j.dump());
}

void OrderQueryHandler::respondBadRequest(HttpResponse* resp, std::string_view message) const {
    json j;
    j["error"] = message;
    resp->setStatusCode(HttpResponse::k400BadRequest);
    resp->setContentType("application/json");
    resp->setBody(j.dump());
}

void OrderQueryHandler::respondServerError(HttpResponse* resp, std::string_view message) const {
    json j;
    j["error"] = message;
    resp->setStatusCode(HttpResponse::k500InternalServerError);
    resp->setContentType("application/json");
    resp->setBody(j.dump());
}

/**
 * ================== 缓存 / 数据库 ==================
 */
std::optional<OrderQueryHandler::Record> OrderQueryHandler::fetchFromCache(const std::string& orderId) const {
    try {
        if (!deps_.cache)
            return std::nullopt;
        return deps_.cache->GetOrder(orderId);
    } catch (const std::exception& e) {
        LOG_WARN("Cache read exception for orderId={}, reason={}", orderId, e.what());
        return std::nullopt;
    }
}

std::optional<OrderQueryHandler::Record> OrderQueryHandler::fetchFromDatabase(const std::string& orderId) const {
    try {
        return deps_.database->GetById(orderId);
    } catch (const std::exception& e) {
        LOG_ERROR("DB query exception for orderId={}, reason={}", orderId, e.what());
        return std::nullopt;
    }
}

OrderQueryHandler::RecordList OrderQueryHandler::fetchListFromCache(const std::string& userId) const {
    try {
        if (!deps_.cache)
            return {};
        auto listOpt = deps_.cache->GetUserOrders(userId);
        if (listOpt)
            return *listOpt;
    } catch (const std::exception& e) {
        LOG_WARN("Cache list read exception for userId={}, reason={}", userId, e.what());
    }
    return {};
}

OrderQueryHandler::RecordList OrderQueryHandler::fetchListFromDatabase(const std::string& userId, std::size_t limit, std::size_t offset) const {
    try {
        return deps_.database->ListByUser(userId, limit, offset);
    } catch (const std::exception& e) {
        LOG_ERROR("DB list query exception for userId={}, reason={}", userId, e.what());
        return {};
    }
}

/**
 * ================== 缓存写入 ==================
 */
void OrderQueryHandler::warmupOrderCache(const Record& record) const {
    if (!deps_.cache)
        return;
    try {
        deps_.cache->PutOrder(record);
        LOG_DEBUG("Order cache warmup success: {}", record.orderId);
    } catch (const std::exception& e) {
        LOG_WARN("Order cache warmup failed: {}, {}", record.orderId, e.what());
    }
}

void OrderQueryHandler::warmupUserCache(const std::string& userId, const RecordList& records) const {
    if (!deps_.cache)
        return;
    try {
        deps_.cache->PutUserOrders(userId, records);
        LOG_DEBUG("User cache warmup success: {}, count={}", userId, records.size());
    } catch (const std::exception& e) {
        LOG_WARN("User cache warmup failed: {}, {}", userId, e.what());
    }
}

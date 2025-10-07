#include "interface/http/OrderCreateHandler.h"

#include <chrono>
#include <format>
#include <stdexcept>
#include <string_view>
#include <sstream>
#include <optional>

#include <nlohmann/json.hpp>

#include "LogMacros.h"
#include "HttpRequest.h"
#include "MQProducer.h"

#include "infra/db/OrderRepository.h"
#include "infra/cache/OrderCache.h"
#include "domain/InventoryService.h"

using json = nlohmann::json;

OrderCreateHandler::OrderCreateHandler(Dependencies deps, Options options) : deps_(std::move(deps)), options_(std::move(options)) {}

void OrderCreateHandler::setIdGenerator(IdGenerator generator) {
    idGenerator_ = std::move(generator);
}

/**
 * 主处理逻辑入口：
 * 解析请求 -> 校验参数 -> 预留库存 -> 持久化订单 -> 缓存 -> MQ -> 响应
 */
void OrderCreateHandler::handle(const HttpRequest& req, HttpResponse* resp) {
    LOG_INFO("Incoming order.create request, content-length={}", req.contentLength());

    if (!ensureDependencies(resp))
        return;

    OrderRepository::OrderRecord record;
    std::string rawPayload;

    if (!parseRequest(req, &record, &rawPayload, resp))
        return;
    if (!validateRecord(record, resp))
        return;

    // 库存预留
    if (options_.requireInventoryReservation) {
        if (!reserveInventory(record, resp))
            return;
    }

    // 数据持久化
    if (!persistOrder(record, rawPayload, resp))
        return;

    // 缓存
    updateCache(record, rawPayload);

    // MQ
    publishOrderEvent(record, rawPayload);

    // 响应
    respondSuccess(record, resp);

    LOG_INFO("Order created successfully, orderId={}, userId={}, productId={}, quantity={}", record.orderId, record.userId, record.productId, record.quantity);
}

/**
 * ========== 内部逻辑 ==========
 */

bool OrderCreateHandler::ensureDependencies(HttpResponse* resp) const {
    if (!deps_.database || !deps_.inventory) {
        LOG_ERROR("Missing mandatory dependency: database or inventory");
        respondError(resp, HttpResponse::k500InternalServerError, "Internal dependency missing (database/inventory)");
        return false;
    }
    return true;
}

bool OrderCreateHandler::parseRequest(const HttpRequest& req, OrderRepository::OrderRecord* outRecord, std::string* rawPayload, HttpResponse* resp) const {
    try {
        const std::string& body = req.body();
        *rawPayload = body;
        json j = json::parse(body, nullptr, false);
        if (j.is_discarded()) {
            LOG_WARN("JSON parse error: {}", body);
            respondError(resp, HttpResponse::k400BadRequest, "Invalid JSON payload");
            return false;
        }

        outRecord->userId = j.value("userId", "");
        outRecord->productId = j.value("productId", "");
        outRecord->quantity = j.value("quantity", 1u);
        outRecord->totalAmount = j.value("amount", 0.0);
        outRecord->currency = j.value("currency", "CNY");
        outRecord->payloadJson = body;

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception while parsing request: {}", e.what());
        respondError(resp, HttpResponse::k400BadRequest, "Malformed request body");
        return false;
    }
}

bool OrderCreateHandler::validateRecord(const OrderRepository::OrderRecord& record, HttpResponse* resp) const {
    if (record.userId.empty() || record.productId.empty()) {
        respondError(resp, HttpResponse::k400BadRequest, "Missing userId or productId");
        return false;
    }
    if (record.quantity == 0 || record.totalAmount <= 0.0) {
        respondError(resp, HttpResponse::k400BadRequest, "Invalid quantity or amount");
        return false;
    }
    return true;
}

bool OrderCreateHandler::reserveInventory(const OrderRepository::OrderRecord& record, HttpResponse* resp) {
    if (!deps_.inventory) {
        respondError(resp, HttpResponse::k500InternalServerError, "Inventory unavailable");
        return false;
    }

    InventoryService::Reservation res;
    std::string errMsg;

    bool ok = deps_.inventory->ReserveForOrder(record, &res, &errMsg);
    if (!ok) {
        LOG_WARN("Inventory reservation failed: user={}, product={}, reason={}", record.userId, record.productId, errMsg);
        respondError(resp, HttpResponse::k503ServiceUnavailable, "Inventory not enough or temporarily unavailable");
        return false;
    }

    LOG_DEBUG("Inventory reserved successfully for order, product={}, qty={}", record.productId, record.quantity);
    return true;
}

bool OrderCreateHandler::persistOrder(OrderRepository::OrderRecord& record, const std::string& rawPayload, HttpResponse* resp) {
    try {
        // 生成订单ID
        record.orderId = idGenerator_ ? idGenerator_() : std::format("ORD-{}", std::chrono::system_clock::now().time_since_epoch().count());
        record.createdAt = Clock::now();
        record.updatedAt = record.createdAt;
        record.status = OrderStatus::kPending;
        record.payloadJson = rawPayload;

        if (!deps_.database->Insert(record)) {
            LOG_ERROR("Order insert failed for orderId={}, productId={}", record.orderId, record.productId);

            // 若库存已预留，尝试释放
            if (options_.requireInventoryReservation && deps_.inventory) {
                InventoryService::Reservation r;
                r.orderId = record.orderId;
                r.productId = record.productId;
                r.quantity = record.quantity;
                deps_.inventory->ReleaseReservation(r, "DB insert failed");
            }

            respondError(resp, HttpResponse::k500InternalServerError, "Failed to persist order");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during persistOrder: {}", e.what());
        respondError(resp, HttpResponse::k500InternalServerError, "Unexpected server error");
        return false;
    }
}

void OrderCreateHandler::updateCache(const OrderRepository::OrderRecord& record, const std::string& rawPayload) {
    if (!options_.enableCache || !deps_.cache)
        return;

    try {
        if (!deps_.cache->PutOrder(record)) {
            LOG_WARN("Cache put failed for orderId={}", record.orderId);
        } else {
            LOG_DEBUG("Order cached successfully: orderId={}", record.orderId);
        }
    } catch (const std::exception& e) {
        LOG_WARN("Cache update exception for orderId={}, reason={}", record.orderId, e.what());
    }
}

void OrderCreateHandler::publishOrderEvent(const OrderRepository::OrderRecord& record, const std::string& rawPayload) {
    if (!options_.enableMqPublish || !deps_.producer)
        return;

    try {
        deps_.producer->publish(options_.mqExchange, options_.mqRoutingKey, rawPayload);
        LOG_DEBUG("Published order.create MQ event for orderId={}", record.orderId);
    } catch (const std::exception& e) {
        LOG_WARN("Failed to publish MQ event: {}", e.what());
    }
}

void OrderCreateHandler::respondSuccess(const OrderRepository::OrderRecord& record, HttpResponse* resp) const {
    json j;
    j["orderId"] = record.orderId;
    j["status"] = ToString(record.status);
    j["message"] = "order created successfully";

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setContentType("application/json");
    resp->setBody(j.dump());
}

void OrderCreateHandler::respondError(HttpResponse* resp, HttpResponse::HttpStatusCode code, std::string_view message) const {
    json j;
    j["error"] = message;
    resp->setStatusCode(code);
    resp->setContentType("application/json");
    resp->setBody(j.dump());

    LOG_WARN("HTTP {} -> {}", static_cast<int>(code), message);
}

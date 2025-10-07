#include "domain/OrderEntity.h"
#include <utility>

OrderEntity::OrderEntity(Record record) :
    orderId_(std::move(record.orderId)),
    userId_(std::move(record.userId)),
    productId_(std::move(record.productId)),
    quantity_(record.quantity),
    totalAmount_(record.totalAmount),
    currency_(std::move(record.currency)),
    payloadJson_(std::move(record.payloadJson)),
    status_(record.status),
    statusReason_(std::move(record.statusReason)),
    createdAt_(record.createdAt),
    updatedAt_(record.updatedAt) {}

OrderEntity OrderEntity::FromRecord(const Record& record) {
    return OrderEntity(record);
}

OrderEntity::Record OrderEntity::ToRecord() const {
    Record rec{};
    rec.orderId = orderId_;
    rec.userId = userId_;
    rec.productId = productId_;
    rec.quantity = quantity_;
    rec.totalAmount = totalAmount_;
    rec.currency = currency_;
    rec.payloadJson = payloadJson_;
    rec.status = status_;
    rec.statusReason = statusReason_;
    rec.createdAt = createdAt_;
    rec.updatedAt = updatedAt_;
    return rec;
}

// ========== 标识信息 ==========
void OrderEntity::SetIdentifiers(std::string orderId, std::string userId, std::string productId) {
    orderId_ = std::move(orderId);
    userId_ = std::move(userId);
    productId_ = std::move(productId);
    Touch(Clock::now());
}

// ========== 业务参数 ==========
void OrderEntity::SetQuantity(std::uint32_t quantity) {
    quantity_ = quantity;
    Touch(Clock::now());
}

void OrderEntity::SetTotalAmount(double amount) {
    totalAmount_ = amount;
    Touch(Clock::now());
}

void OrderEntity::SetCurrency(std::string currency) {
    currency_ = std::move(currency);
    Touch(Clock::now());
}

void OrderEntity::SetPayload(std::string payload) {
    payloadJson_ = std::move(payload);
    Touch(Clock::now());
}

// ========== 状态机 ==========
void OrderEntity::MarkPending(std::string reason) {
    setStatus(OrderStatus::kPending, std::move(reason));
}

void OrderEntity::MarkProcessing(std::string reason) {
    setStatus(OrderStatus::kProcessing, std::move(reason));
}

void OrderEntity::MarkReserved(std::string reason) {
    setStatus(OrderStatus::kReserved, std::move(reason));
}

void OrderEntity::MarkPaid(double amount, Clock::time_point paidAt, std::string reason) {
    paidAmount_ = amount;
    paidAt_ = paidAt;
    setStatus(OrderStatus::kPaid, std::move(reason));
}

void OrderEntity::MarkCompleted(std::string reason) {
    setStatus(OrderStatus::kCompleted, std::move(reason));
}

void OrderEntity::MarkCancelled(std::string reason) {
    setStatus(OrderStatus::kCancelled, std::move(reason));
}

void OrderEntity::MarkFailed(std::string reason) {
    setStatus(OrderStatus::kFailed, std::move(reason));
}

// ========== 状态判定 ==========
bool OrderEntity::IsPending() const noexcept {
    return status_ == OrderStatus::kPending;
}

bool OrderEntity::IsReservable() const noexcept {
    return status_ == OrderStatus::kPending || status_ == OrderStatus::kProcessing;
}

bool OrderEntity::IsTerminal() const noexcept {
    switch (status_) {
    case OrderStatus::kCompleted:
    case OrderStatus::kCancelled:
    case OrderStatus::kFailed:
        return true;
    default:
        return false;
    }
}

// ========== 时间与状态管理 ==========
void OrderEntity::Touch(Clock::time_point ts) {
    updatedAt_ = ts;
}

void OrderEntity::SetCreatedAt(Clock::time_point ts) {
    createdAt_ = ts;
    updatedAt_ = ts;
}

void OrderEntity::setStatus(OrderStatus status, std::string reason) {
    status_ = status;
    statusReason_ = std::move(reason);
    updatedAt_ = Clock::now();
}

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "infra/db/OrderRepository.h"

/**
 * @brief OrderEntity：领域层订单聚合根
 *
 * 该对象代表业务语义下的订单实体，提供状态流转与持久化映射能力。
 * OrderRepository::OrderRecord 只关注数据结构，OrderEntity 则封装业务规则。
 */
class OrderEntity {
public:
    using Clock = std::chrono::system_clock;
    using Record = OrderRepository::OrderRecord;

    OrderEntity() = default;
    explicit OrderEntity(Record record);

    static OrderEntity FromRecord(const Record& record);
    Record ToRecord() const;

    // ========= 标识信息 =========
    const std::string& id() const noexcept { return orderId_; }
    const std::string& userId() const noexcept { return userId_; }
    const std::string& productId() const noexcept { return productId_; }

    void SetIdentifiers(std::string orderId, std::string userId, std::string productId);

    // ========= 业务参数 =========
    std::uint32_t quantity() const noexcept { return quantity_; }
    double totalAmount() const noexcept { return totalAmount_; }
    const std::string& currency() const noexcept { return currency_; }

    void SetQuantity(std::uint32_t quantity);
    void SetTotalAmount(double amount);
    void SetCurrency(std::string currency);

    const std::string& payload() const noexcept { return payloadJson_; }
    void SetPayload(std::string payload);

    // ========= 状态机 =========
    OrderStatus status() const noexcept { return status_; }
    const std::string& statusReason() const noexcept { return statusReason_; }
    std::optional<double> paidAmount() const noexcept { return paidAmount_; }
    std::optional<Clock::time_point> paidAt() const noexcept { return paidAt_; }

    void MarkPending(std::string reason = {});
    void MarkProcessing(std::string reason = {});
    void MarkReserved(std::string reason = {});
    void MarkPaid(double amount, Clock::time_point paidAt, std::string reason = {});
    void MarkCompleted(std::string reason = {});
    void MarkCancelled(std::string reason);
    void MarkFailed(std::string reason);

    bool IsPending() const noexcept;
    bool IsReservable() const noexcept;
    bool IsTerminal() const noexcept;

    // ========= 时间信息 =========
    Clock::time_point createdAt() const noexcept { return createdAt_; }
    Clock::time_point updatedAt() const noexcept { return updatedAt_; }

    void Touch(Clock::time_point ts);
    void SetCreatedAt(Clock::time_point ts);

private:
    void setStatus(OrderStatus status, std::string reason);

private:
    std::string orderId_;
    std::string userId_;
    std::string productId_;
    std::uint32_t quantity_{1};
    double totalAmount_{0.0};
    std::string currency_{"CNY"};
    std::string payloadJson_;

    OrderStatus status_{OrderStatus::kPending};
    std::string statusReason_;
    std::optional<double> paidAmount_{};
    std::optional<Clock::time_point> paidAt_{};

    Clock::time_point createdAt_{};
    Clock::time_point updatedAt_{};
};

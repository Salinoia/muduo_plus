#pragma once

#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class MySQLConnPool;

namespace sql {
class ResultSet;
}  // namespace sql

enum class OrderStatus : std::uint8_t { kPending = 0, kProcessing, kReserved, kPaid, kCompleted, kCancelled, kFailed };

std::string ToString(OrderStatus status);
OrderStatus OrderStatusFromString(std::string_view status);

class OrderRepository {
public:
    struct OrderRecord {
        std::string orderId;
        std::string userId;
        std::string productId;
        std::uint32_t quantity{1};
        double totalAmount{0.0};
        std::string currency{"CNY"};
        OrderStatus status{OrderStatus::kPending};
        std::string statusReason;
        std::string payloadJson;
        std::chrono::system_clock::time_point createdAt{};
        std::chrono::system_clock::time_point updatedAt{};
    };

    using Record = OrderRecord;
    using RecordList = std::vector<Record>;
    using Clock = std::chrono::system_clock;

    OrderRepository(std::shared_ptr<MySQLConnPool> pool, std::string tableName = "orders");

    std::shared_ptr<MySQLConnPool> pool() const noexcept { return pool_; }
    const std::string& tableName() const noexcept { return tableName_; }

    void EnsureSchema();

    bool Insert(const OrderRecord& record);
    bool Upsert(const OrderRecord& record);
    bool UpdateStatus(const std::string& orderId, OrderStatus status, const std::string& reason = {});
    bool UpdatePayment(const std::string& orderId, double paidAmount, Clock::time_point paidAt);
    bool UpdatePayload(const std::string& orderId, const std::string& payloadJson);
    bool Touch(const std::string& orderId, Clock::time_point ts);
    bool Remove(const std::string& orderId);

    std::optional<OrderRecord> GetById(const std::string& orderId);
    RecordList ListByUser(const std::string& userId, std::size_t limit = 20, std::size_t offset = 0);
    RecordList ListRecent(std::size_t limit = 20);

    std::future<bool> InsertAsync(OrderRecord record);
    std::future<bool> UpsertAsync(OrderRecord record);
    std::future<std::optional<OrderRecord>> GetByIdAsync(std::string orderId);
    std::future<RecordList> ListByUserAsync(std::string userId, std::size_t limit = 20, std::size_t offset = 0);
    std::future<RecordList> ListRecentAsync(std::size_t limit = 20);
    std::future<bool> UpdateStatusAsync(std::string orderId, OrderStatus status, std::string reason = {});

private:
    OrderRecord ParseSingle(sql::ResultSet* rs) const;
    RecordList ParseMany(sql::ResultSet* rs) const;

    std::string BuildInsertSQL(const OrderRecord& record) const;
    std::string BuildUpsertSQL(const OrderRecord& record) const;
    std::string BuildUpdateStatusSQL(const std::string& orderId, OrderStatus status, const std::string& reason) const;
    std::string BuildUpdatePaymentSQL(const std::string& orderId, double paidAmount, Clock::time_point paidAt) const;
    std::string BuildUpdatePayloadSQL(const std::string& orderId, const std::string& payloadJson) const;
    std::string BuildTouchSQL(const std::string& orderId, Clock::time_point ts) const;
    std::string BuildDeleteSQL(const std::string& orderId) const;
    std::string BuildSelectByIdSQL(const std::string& orderId) const;
    std::string BuildSelectByUserSQL(const std::string& userId, std::size_t limit, std::size_t offset) const;
    std::string BuildSelectRecentSQL(std::size_t limit) const;

    std::shared_ptr<MySQLConnPool> pool_;
    std::string tableName_;
    bool schemaEnsured_{false};
};

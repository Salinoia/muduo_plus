#include "infra/db/OrderRepository.h"
#include "MySQLConnPool.h"

#include <cppconn/resultset.h>
#include <future>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <ctime>

// ------------------- 枚举与时间辅助 -------------------

std::string ToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::kPending:    return "Pending";
        case OrderStatus::kProcessing: return "Processing";
        case OrderStatus::kReserved:   return "Reserved";
        case OrderStatus::kPaid:       return "Paid";
        case OrderStatus::kCompleted:  return "Completed";
        case OrderStatus::kCancelled:  return "Cancelled";
        case OrderStatus::kFailed:     return "Failed";
    }
    return "Unknown";
}

OrderStatus OrderStatusFromString(std::string_view s) {
    if (s == "Pending")    return OrderStatus::kPending;
    if (s == "Processing") return OrderStatus::kProcessing;
    if (s == "Reserved")   return OrderStatus::kReserved;
    if (s == "Paid")       return OrderStatus::kPaid;
    if (s == "Completed")  return OrderStatus::kCompleted;
    if (s == "Cancelled")  return OrderStatus::kCancelled;
    if (s == "Failed")     return OrderStatus::kFailed;
    return OrderStatus::kPending;
}

namespace {
std::string TimeToSQL(std::chrono::system_clock::time_point tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream oss;
    oss << "'" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "'";
    return oss.str();
}
} // namespace

// ------------------- 构造与模式初始化 -------------------

OrderRepository::OrderRepository(std::shared_ptr<MySQLConnPool> pool, std::string tableName)
    : pool_(std::move(pool)), tableName_(std::move(tableName)) {
    if (!pool_) {
        throw std::invalid_argument("OrderRepository: MySQLConnPool cannot be null");
    }
}

void OrderRepository::EnsureSchema() {
    if (schemaEnsured_) return;
    std::ostringstream oss;
    oss << "CREATE TABLE IF NOT EXISTS " << tableName_ << " ("
        << "order_id VARCHAR(64) PRIMARY KEY,"
        << "user_id VARCHAR(64) NOT NULL,"
        << "product_id VARCHAR(64) NOT NULL,"
        << "quantity INT NOT NULL,"
        << "total_amount DOUBLE NOT NULL,"
        << "currency VARCHAR(16) NOT NULL,"
        << "status VARCHAR(32) NOT NULL,"
        << "status_reason VARCHAR(255),"
        << "payload_json TEXT,"
        << "created_at DATETIME NOT NULL,"
        << "updated_at DATETIME NOT NULL)";
    auto fut = pool_->SubmitUpdate(oss.str());
    fut.get();
    schemaEnsured_ = true;
}

// ------------------- 同步接口实现 -------------------

bool OrderRepository::Insert(const Record& record) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildInsertSQL(record)).get() > 0;
}

bool OrderRepository::Upsert(const Record& record) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildUpsertSQL(record)).get() > 0;
}

bool OrderRepository::UpdateStatus(const std::string& orderId, OrderStatus status, const std::string& reason) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildUpdateStatusSQL(orderId, status, reason)).get() > 0;
}

bool OrderRepository::UpdatePayment(const std::string& orderId, double paidAmount, Clock::time_point paidAt) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildUpdatePaymentSQL(orderId, paidAmount, paidAt)).get() > 0;
}

bool OrderRepository::UpdatePayload(const std::string& orderId, const std::string& payloadJson) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildUpdatePayloadSQL(orderId, payloadJson)).get() > 0;
}

bool OrderRepository::Touch(const std::string& orderId, Clock::time_point ts) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildTouchSQL(orderId, ts)).get() > 0;
}

bool OrderRepository::Remove(const std::string& orderId) {
    if (!pool_) return false;
    return pool_->SubmitUpdate(BuildDeleteSQL(orderId)).get() > 0;
}

std::optional<OrderRepository::Record> OrderRepository::GetById(const std::string& orderId) {
    if (!pool_) return std::nullopt;
    auto fut = pool_->SubmitQuery(BuildSelectByIdSQL(orderId));
    auto rs = fut.get();
    if (!rs || !rs->next()) return std::nullopt;
    return ParseSingle(rs.get());
}

OrderRepository::RecordList OrderRepository::ListByUser(const std::string& userId, std::size_t limit, std::size_t offset) {
    if (!pool_) return {};
    auto fut = pool_->SubmitQuery(BuildSelectByUserSQL(userId, limit, offset));
    auto rs = fut.get();
    return rs ? ParseMany(rs.get()) : RecordList{};
}

OrderRepository::RecordList OrderRepository::ListRecent(std::size_t limit) {
    if (!pool_) return {};
    auto fut = pool_->SubmitQuery(BuildSelectRecentSQL(limit));
    auto rs = fut.get();
    return rs ? ParseMany(rs.get()) : RecordList{};
}

// ------------------- 异步接口实现 -------------------

std::future<bool> OrderRepository::InsertAsync(Record record) {
    return pool_->SubmitExec(BuildInsertSQL(record));
}

std::future<bool> OrderRepository::UpsertAsync(Record record) {
    return pool_->SubmitExec(BuildUpsertSQL(record));
}

std::future<std::optional<OrderRepository::Record>> OrderRepository::GetByIdAsync(std::string orderId) {
    return std::async(std::launch::async, [this, id = std::move(orderId)]() -> std::optional<Record> {
        auto fut = pool_->SubmitQuery(BuildSelectByIdSQL(id));
        auto rs = fut.get();
        if (!rs || !rs->next()) return std::nullopt;
        return ParseSingle(rs.get());
    });
}

std::future<OrderRepository::RecordList> OrderRepository::ListByUserAsync(std::string userId, std::size_t limit, std::size_t offset) {
    return std::async(std::launch::async, [this, uid = std::move(userId), limit, offset]() -> RecordList {
        auto fut = pool_->SubmitQuery(BuildSelectByUserSQL(uid, limit, offset));
        auto rs = fut.get();
        return rs ? ParseMany(rs.get()) : RecordList{};
    });
}

std::future<OrderRepository::RecordList> OrderRepository::ListRecentAsync(std::size_t limit) {
    return std::async(std::launch::async, [this, limit]() -> RecordList {
        auto fut = pool_->SubmitQuery(BuildSelectRecentSQL(limit));
        auto rs = fut.get();
        return rs ? ParseMany(rs.get()) : RecordList{};
    });
}

std::future<bool> OrderRepository::UpdateStatusAsync(std::string orderId, OrderStatus status, std::string reason) {
    return pool_->SubmitExec(BuildUpdateStatusSQL(orderId, status, reason));
}

// ------------------- SQL 构造与结果解析 -------------------

std::string OrderRepository::BuildInsertSQL(const Record& r) const {
    std::ostringstream oss;
    oss << "INSERT INTO " << tableName_
        << " (order_id,user_id,product_id,quantity,total_amount,currency,status,status_reason,payload_json,created_at,updated_at)"
        << " VALUES ('" << r.orderId << "','" << r.userId << "','" << r.productId << "',"
        << r.quantity << "," << r.totalAmount << ",'" << r.currency << "','" << ToString(r.status)
        << "','" << r.statusReason << "','" << r.payloadJson << "'," << TimeToSQL(r.createdAt)
        << "," << TimeToSQL(r.updatedAt) << ")";
    return oss.str();
}

std::string OrderRepository::BuildUpsertSQL(const Record& r) const {
    std::ostringstream oss;
    oss << BuildInsertSQL(r)
        << " ON DUPLICATE KEY UPDATE "
        << "status=VALUES(status),status_reason=VALUES(status_reason),payload_json=VALUES(payload_json),updated_at=VALUES(updated_at)";
    return oss.str();
}

std::string OrderRepository::BuildUpdateStatusSQL(const std::string& id, OrderStatus s, const std::string& reason) const {
    std::ostringstream oss;
    oss << "UPDATE " << tableName_
        << " SET status='" << ToString(s) << "', status_reason='" << reason
        << "', updated_at=NOW() WHERE order_id='" << id << "'";
    return oss.str();
}

std::string OrderRepository::BuildUpdatePaymentSQL(const std::string& id, double paid, Clock::time_point paidAt) const {
    std::ostringstream oss;
    oss << "UPDATE " << tableName_
        << " SET status='Paid', total_amount=" << paid << ", updated_at=" << TimeToSQL(paidAt)
        << " WHERE order_id='" << id << "'";
    return oss.str();
}

std::string OrderRepository::BuildUpdatePayloadSQL(const std::string& id, const std::string& payload) const {
    std::ostringstream oss;
    oss << "UPDATE " << tableName_
        << " SET payload_json='" << payload << "', updated_at=NOW() WHERE order_id='" << id << "'";
    return oss.str();
}

std::string OrderRepository::BuildTouchSQL(const std::string& id, Clock::time_point ts) const {
    std::ostringstream oss;
    oss << "UPDATE " << tableName_ << " SET updated_at=" << TimeToSQL(ts)
        << " WHERE order_id='" << id << "'";
    return oss.str();
}

std::string OrderRepository::BuildDeleteSQL(const std::string& id) const {
    return "DELETE FROM " + tableName_ + " WHERE order_id='" + id + "'";
}

std::string OrderRepository::BuildSelectByIdSQL(const std::string& id) const {
    return "SELECT * FROM " + tableName_ + " WHERE order_id='" + id + "'";
}

std::string OrderRepository::BuildSelectByUserSQL(const std::string& uid, std::size_t limit, std::size_t offset) const {
    std::ostringstream oss;
    oss << "SELECT * FROM " << tableName_
        << " WHERE user_id='" << uid << "' ORDER BY created_at DESC "
        << "LIMIT " << limit << " OFFSET " << offset;
    return oss.str();
}

std::string OrderRepository::BuildSelectRecentSQL(std::size_t limit) const {
    std::ostringstream oss;
    oss << "SELECT * FROM " << tableName_
        << " ORDER BY created_at DESC LIMIT " << limit;
    return oss.str();
}

// ------------------- 结果集解析 -------------------

OrderRepository::Record OrderRepository::ParseSingle(sql::ResultSet* rs) const {
    auto safeString = [rs](const char* column) -> std::string {
        return rs->isNull(column) ? std::string{} : rs->getString(column).asStdString();
    };

    Record r;
    r.orderId      = safeString("order_id");
    r.userId       = safeString("user_id");
    r.productId    = safeString("product_id");
    r.quantity     = rs->getInt("quantity");
    r.totalAmount  = static_cast<double>(rs->getDouble("total_amount"));
    r.currency     = safeString("currency");
    r.status       = OrderStatusFromString(safeString("status"));
    r.statusReason = safeString("status_reason");
    r.payloadJson  = safeString("payload_json");
    return r;
}

OrderRepository::RecordList OrderRepository::ParseMany(sql::ResultSet* rs) const {
    RecordList list;
    while (rs->next()) {
        list.push_back(ParseSingle(rs));
    }
    return list;
}

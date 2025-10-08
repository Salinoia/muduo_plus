#include "MySQLConn.h"

#include <cppconn/exception.h>
#include <cppconn/statement.h>

#include "LogMacros.h"

// ------------------ MySQLConn ------------------
MySQLConn::MySQLConn(const MySQLConnInfo& info) : info_(info) {
    driver_ = get_driver_instance();
}

MySQLConn::~MySQLConn() noexcept {
    Close();
}

// ✅ 新版：支持重试 + 日志输出
bool MySQLConn::Open(int maxRetries, int retryDelaySec) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            conn_.reset(driver_->connect(info_.url, info_.user, info_.password));
            if (!conn_) {
                LOG_ERROR("[MySQLConn] driver_->connect() returned null (attempt {}/{})", attempt + 1, maxRetries);
                continue;
            }
            conn_->setSchema(info_.database);
            LOG_INFO("[MySQLConn] Connected successfully to {}", info_.url);
            return true;
        } catch (const sql::SQLException& e) {
            LOG_ERROR("[MySQLConn] Connection failed ({}/{}): {} (Code: {}, SQLState: {})", attempt + 1, maxRetries, e.what(), e.getErrorCode(), e.getSQLStateCStr());
            if (attempt + 1 == maxRetries) {
                LOG_FATAL("[MySQLConn] Reached max retries ({}), giving up.", maxRetries);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::seconds(retryDelaySec));
        }
    }
    return false;
}

// 保留原有无参数版（默认 3 次）
bool MySQLConn::Open() {
    return Open(3, 2);
}

void MySQLConn::Close() noexcept {
    if (conn_) {
        try {
            conn_->close();
        } catch (...) {
            LOG_WARN("[MySQLConn] Exception during close() ignored.");
        }
        conn_.reset();
    }
}

// ------------------ SQL 操作 ------------------
std::unique_ptr<sql::ResultSet> MySQLConn::ExecuteQuery(const std::string& sql) {
    if (!conn_)
        return nullptr;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        return std::unique_ptr<sql::ResultSet>(stmt->executeQuery(sql));
    } catch (const sql::SQLException& e) {
        LOG_ERROR("[MySQLConn] Query failed: {} (Code: {}, SQLState: {})", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return nullptr;
    }
}

bool MySQLConn::ExecuteStatement(const std::string& sql) {
    if (!conn_)
        return false;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        stmt->execute(sql);
        return true;
    } catch (const sql::SQLException& e) {
        LOG_ERROR("[MySQLConn] Execute failed: {} (Code: {}, SQLState: {})", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return false;
    }
}

int MySQLConn::ExecuteUpdate(const std::string& sql) {
    if (!conn_)
        return -1;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        return stmt->executeUpdate(sql);
    } catch (const sql::SQLException& e) {
        LOG_ERROR("[MySQLConn] Update failed: {} (Code: {}, SQLState: {})", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
    }
}
// ------------------ MySQLWorker ------------------
MySQLWorker::MySQLWorker(std::shared_ptr<MySQLConn> conn, std::shared_ptr<BlockingQueue<std::shared_ptr<SQLOperation>>> queue) : conn_(std::move(conn)), queue_(std::move(queue)) {}

MySQLWorker::~MySQLWorker() {
    Stop();
}

void MySQLWorker::Start() {
    running_ = true;
    thread_ = std::thread(&MySQLWorker::WorkerLoop, this);
}

void MySQLWorker::Stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void MySQLWorker::WorkerLoop() {
    std::shared_ptr<SQLOperation> task;
    while (running_ && queue_->Pop(task)) {
        if (!task)
            continue;
        task->Execute(conn_.get());
    }
}

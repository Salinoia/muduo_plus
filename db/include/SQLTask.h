#pragma once
#include <cppconn/resultset.h>

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sql {
class ResultSet;
}
class MySQLConn;

// ------------------ SQL 异步任务 ------------------
enum class SQLKind { Query, Exec, Update };
class SQLOperation {
public:
    SQLOperation(std::string sql) : kind_(SQLKind::Query), sql_(std::move(sql)) {}
    explicit SQLOperation(SQLKind kind, std::string sql) : kind_(kind), sql_(std::move(sql)) {}
    void Execute(MySQLConn* conn);

    std::future<std::unique_ptr<sql::ResultSet>> GetFuture() { return promise_.get_future(); }
    std::future<bool> GetFutureBool() { return promise_bool_.get_future(); }
    std::future<int> GetFutureInt() { return promise_int_.get_future(); }
private:
    SQLKind kind_;
    std::string sql_;
    std::promise<std::unique_ptr<sql::ResultSet>> promise_;
    std::promise<bool> promise_bool_;
    std::promise<int> promise_int_;
};

// ------------------ 回调包装 ------------------
class QueryCallback {
public:
    QueryCallback(std::future<std::unique_ptr<sql::ResultSet>>&& future, std::function<void(std::unique_ptr<sql::ResultSet>)>&& cb) : future_(std::move(future)), callback_(std::move(cb)) {}

    bool InvokeIfReady() {
        if (future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            // 禁止使用 std::move 加右值，违背了返回值优化原则
            callback_(future_.get());
            return true;
        }
        return false;
    }

private:
    std::future<std::unique_ptr<sql::ResultSet>> future_;
    std::function<void(std::unique_ptr<sql::ResultSet>)> callback_;
};

// ------------------ 异步回调调度器 ------------------
class AsyncProcessor {
public:
    void AddQueryCallback(QueryCallback&& cb) {
        std::scoped_lock lock(mtx_);
        pending_.emplace_back(std::move(cb));
    }

    void InvokeIfReady() {
        std::scoped_lock lock(mtx_);
        auto it = pending_.begin();
        while (it != pending_.end()) {
            if (it->InvokeIfReady())
                it = pending_.erase(it);
            else
                ++it;
        }
    }

private:
    std::vector<QueryCallback> pending_;
    std::mutex mtx_;
};

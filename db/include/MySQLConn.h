#pragma once
#include <cppconn/connection.h>
#include <cppconn/driver.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "BlockingQueue.h"
#include "MySQLConnInfo.h"
#include "SQLTask.h"

// ------------------ 数据库连接 ------------------
class MySQLConn {
public:
    explicit MySQLConn(const MySQLConnInfo& info);
    ~MySQLConn() noexcept;

    bool Open();
    bool Open(int maxRetries, int retryDelaySec);
    
    void Close() noexcept;

    std::unique_ptr<sql::ResultSet> ExecuteQuery(const std::string& sql);
    bool ExecuteStatement(const std::string& sql);
    int ExecuteUpdate(const std::string& sql);

    bool IsOpen() const noexcept { return conn_ != nullptr; }
    
    bool Ping() noexcept;
    bool IsAlive() const noexcept { return alive_.load(std::memory_order_relaxed); }

private:

    sql::Driver* driver_{nullptr};
    std::unique_ptr<sql::Connection> conn_;
    MySQLConnInfo info_;
    std::atomic<bool> alive_{true}; 
    std::mutex conn_mtx_;
};

// ------------------ 异步执行线程 ------------------
class MySQLWorker {
public:
    MySQLWorker(std::shared_ptr<MySQLConn> conn, std::shared_ptr<BlockingQueue<std::shared_ptr<SQLOperation>>> queue);
    ~MySQLWorker();

    void Start();
    void Stop();

private:
    void WorkerLoop();

    std::shared_ptr<MySQLConn> conn_;
    std::shared_ptr<BlockingQueue<std::shared_ptr<SQLOperation>>> queue_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

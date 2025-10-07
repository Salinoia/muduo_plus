#include "MySQLConnPool.h"

#include <cppconn/exception.h>

#include <iostream>

// ------------------ 静态成员定义 ------------------
std::unordered_map<std::string, std::weak_ptr<MySQLConnPool>> MySQLConnPool::instances_;
std::mutex MySQLConnPool::instance_mtx_;

// ------------------ 构造与析构 ------------------
MySQLConnPool::MySQLConnPool(const std::string& db) : database_(db) {}

MySQLConnPool::~MySQLConnPool() {
    Shutdown();
}

// ------------------ 单例获取 ------------------
std::shared_ptr<MySQLConnPool> MySQLConnPool::GetInstance(const std::string& db) {
    std::lock_guard<std::mutex> lock(instance_mtx_);
    auto it = instances_.find(db);
    if (it != instances_.end()) {
        if (auto ptr = it->second.lock())
            return ptr;
    }
    auto pool = std::shared_ptr<MySQLConnPool>(new MySQLConnPool(db));
    instances_[db] = pool;
    return pool;
}

// ------------------ 初始化连接池 ------------------
void MySQLConnPool::InitPool(const MySQLConnInfo& info, int initial_size, int max_size, int max_idle_time, int connect_timeout) {
    initial_size_ = initial_size;
    max_size_ = max_size;
    max_idle_time_ = max_idle_time;
    connect_timeout_ = connect_timeout;

    queue_ = std::make_shared<BlockingQueue<std::shared_ptr<SQLOperation>>>();

    CreateInitialConnections(info);
    SpawnWorkerThreads();

    StartKeepAlive();
}

void MySQLConnPool::CreateInitialConnections(const MySQLConnInfo& info) {
    std::lock_guard<std::mutex> lock(pool_mtx_);
    for (int i = 0; i < initial_size_; ++i) {
        auto conn = std::make_shared<MySQLConn>(info);
        if (conn->Open())
            conns_.push_back(conn);
    }
}

void MySQLConnPool::SpawnWorkerThreads() {
    std::lock_guard<std::mutex> lock(pool_mtx_);
    for (auto& conn : conns_) {
        auto worker = std::make_unique<MySQLWorker>(conn, queue_);
        worker->Start();
        workers_.push_back(std::move(worker));
    }
}

// ------------------ 提交异步任务 ------------------
QueryCallback MySQLConnPool::AsyncQuery(const std::string& sql, std::function<void(std::unique_ptr<sql::ResultSet>)>&& callback) {
    auto operation = std::make_shared<SQLOperation>(sql);
    auto future = operation->GetFuture();
    queue_->Push(operation);
    return QueryCallback(std::move(future), std::move(callback));
}

std::future<std::unique_ptr<sql::ResultSet>> MySQLConnPool::SubmitQuery(const std::string& sql) {
    auto operation = std::make_shared<SQLOperation>(sql);
    auto future = operation->GetFuture();
    queue_->Push(operation);
    return future;
}

std::future<bool> MySQLConnPool::SubmitExec(const std::string& sql) {
    auto op = std::make_shared<SQLOperation>(SQLKind::Exec, sql);
    auto fut = op->GetFutureBool();   
    queue_->Push(op);
    return fut;
}

std::future<int> MySQLConnPool::SubmitUpdate(const std::string& sql) {
    auto op = std::make_shared<SQLOperation>(SQLKind::Update, sql);
    auto fut = op->GetFutureInt();    
    queue_->Push(op);
    return fut;
}


// ------------------ 停止所有线程 ------------------
void MySQLConnPool::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(pool_mtx_);
        if (workers_.empty())
            return;
        queue_->Cancel();
        for (auto& w : workers_)
            w->Stop();
        workers_.clear();
        conns_.clear();
    }
    StopKeepAlive();  // 停止心跳线程
    std::cout << "[MySQLConnPool] Shutdown completed for " << database_ << std::endl;
}

void MySQLConnPool::StartKeepAlive() {
    running_ = true;
    keepalive_thread_ = std::thread(&MySQLConnPool::KeepAliveLoop, this);
}

void MySQLConnPool::StopKeepAlive() {
    running_ = false;
    if (keepalive_thread_.joinable())
        keepalive_thread_.join();
}

void MySQLConnPool::KeepAliveLoop() {
    using namespace std::chrono_literals;
    const auto interval = std::chrono::seconds(std::max(5, max_idle_time_ / 2));

    while (running_) {
        {
            std::lock_guard<std::mutex> lock(pool_mtx_);
            for (auto& conn : conns_) {
                if (!conn->IsOpen() || !conn->ExecuteQuery("SELECT 1;")) {
                    std::cerr << "[KeepAlive] Reconnecting to MySQL..." << std::endl;
                    conn->Close();
                    conn->Open();
                }
            }
        }
        std::this_thread::sleep_for(interval);
    }
}

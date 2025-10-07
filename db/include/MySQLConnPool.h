#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "BlockingQueue.h"
#include "MySQLConn.h"
#include "SQLTask.h"

// ------------------ 异步连接池 ------------------
class MySQLConnPool {
public:
    explicit MySQLConnPool(const std::string& db);
    ~MySQLConnPool();
    // 获取指定数据库的连接池实例
    static std::shared_ptr<MySQLConnPool> GetInstance(const std::string& db);

    // 初始化连接池
    void InitPool(const MySQLConnInfo& info, int initial_size = 4, int max_size = 32, int max_idle_time = 60, int connect_timeout = 5);

    // 提交异步查询任务
    QueryCallback AsyncQuery(const std::string& sql, std::function<void(std::unique_ptr<sql::ResultSet>)>&& callback);

    std::future<std::unique_ptr<sql::ResultSet>> SubmitQuery(const std::string& sql);
    std::future<bool> SubmitExec(const std::string&);
    std::future<int> SubmitUpdate(const std::string&);

    // 停止所有 worker 线程
    void Shutdown();

private:
    void CreateInitialConnections(const MySQLConnInfo& info);
    void SpawnWorkerThreads();

    // ------------------ 新增：心跳检测 ------------------
    void KeepAliveLoop();
    void StartKeepAlive();
    void StopKeepAlive();

    std::thread keepalive_thread_;
    std::atomic<bool> running_{false};
    // ---------------------------------------------------

    std::string database_;
    std::vector<std::shared_ptr<MySQLConn>> conns_;
    std::vector<std::unique_ptr<MySQLWorker>> workers_;
    std::shared_ptr<BlockingQueue<std::shared_ptr<SQLOperation>>> queue_;

    int initial_size_ = 0;
    int max_size_ = 0;
    int max_idle_time_ = 0;
    int connect_timeout_ = 0;

    static std::unordered_map<std::string, std::weak_ptr<MySQLConnPool>> instances_;
    static std::mutex instance_mtx_;
    std::mutex pool_mtx_;
};

#include "MySQLConn.h"

#include <cppconn/exception.h>
#include <cppconn/statement.h>

#include <iostream>

// ------------------ MySQLConn ------------------
MySQLConn::MySQLConn(const MySQLConnInfo& info) : info_(info) {
    driver_ = get_driver_instance();
}

MySQLConn::~MySQLConn() noexcept {
    Close();
}

bool MySQLConn::Open() {
    try {
        conn_.reset(driver_->connect(info_.url, info_.user, info_.password));
        if (!conn_)
            return false;
        conn_->setSchema(info_.database);
        return true;
    } catch (const sql::SQLException& e) {
        std::cerr << "[MySQLConn] Connection failed: " << e.what() << " (Code: " << e.getErrorCode() << ", SQLState: " << e.getSQLStateCStr() << ")" << std::endl;
        return false;
    }
}

void MySQLConn::Close() noexcept {
    if (conn_) {
        try {
            conn_->close();
        } catch (...) {}
        conn_.reset();
    }
}

std::unique_ptr<sql::ResultSet> MySQLConn::ExecuteQuery(const std::string& sql) {
    if (!conn_)
        return nullptr;
    try {
        std::unique_ptr<sql::Statement> stmt(conn_->createStatement());
        return std::unique_ptr<sql::ResultSet>(stmt->executeQuery(sql));
    } catch (const sql::SQLException& e) {
        std::cerr << "[MySQLConn] Query failed: " << e.what() << " (Code: " << e.getErrorCode() << ", SQLState: " << e.getSQLStateCStr() << ")" << std::endl;
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
        std::cerr << "[MySQLConn] Execute failed: " << e.what() << " (Code: " << e.getErrorCode() << ", SQLState: " << e.getSQLStateCStr() << ")" << std::endl;
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
        std::cerr << "[MySQLConn] Update failed: " << e.what() << " (Code: " << e.getErrorCode() << ", SQLState: " << e.getSQLStateCStr() << ")" << std::endl;
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

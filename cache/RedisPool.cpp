#include "RedisPool.h"

#include <iostream>

// 构造函数
RedisPool::RedisPool(const std::string& host, int port, size_t pool_size, const std::string& password, int timeout_ms) : host_(host), port_(port), password_(password) {
    timeout_.tv_sec = timeout_ms / 1000;
    timeout_.tv_usec = (timeout_ms % 1000) * 1000;

    for (size_t i = 0; i < pool_size; ++i) {
        auto client = std::make_unique<RedisClient>(host_, port_, password_, timeout_);
        if (client->Connect()) {
            clients_.push(std::move(client));
        } else {
            std::cerr << "[RedisPool] Failed to init client " << i << std::endl;
        }
    }
}

// 析构函数
RedisPool::~RedisPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!clients_.empty()) {
        clients_.pop();
    }
}

// 获取连接
std::shared_ptr<RedisClient> RedisPool::GetClient() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() { return !clients_.empty(); });

    auto client = std::move(clients_.front());
    clients_.pop();

    auto self = shared_from_this();
    return std::shared_ptr<RedisClient>(client.release(), [self](RedisClient* ptr) { self->Release(ptr); });
}

// 归还连接
void RedisPool::Release(RedisClient* client) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!client->IsConnected())
        client->Connect();
    clients_.push(std::unique_ptr<RedisClient>(client));
    lock.unlock();
    cond_.notify_one();
}

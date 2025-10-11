#include "RedisPool.h"
#include "LogMacros.h"

RedisPool::RedisPool(const std::string& host, int port, size_t pool_size, const std::string& password, int timeout_ms) : host_(host), port_(port), password_(password) {
    timeout_.tv_sec = timeout_ms / 1000;
    timeout_.tv_usec = (timeout_ms % 1000) * 1000;

    LOG_INFO("[RedisPool] Initializing pool -> {}:{} (size = {})", host_, port_, pool_size);

    for (size_t i = 0; i < pool_size; ++i) {
        auto client = std::make_unique<RedisClient>(host_, port_, password_, timeout_);
        if (client->Connect()) {
            LOG_INFO("[RedisPool] Client {} connected", i);
            clients_.push(std::move(client));
        } else {
            LOG_ERROR("[RedisPool] Failed to initialize client {}", i);
        }
    }

    LOG_INFO("[RedisPool] Initialization complete ({} clients ready)", clients_.size());
}

RedisPool::~RedisPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG_INFO("[RedisPool] Destroying pool ({} clients)", clients_.size());
    while (!clients_.empty()) {
        clients_.pop();
    }
}

std::shared_ptr<RedisClient> RedisPool::GetClient() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() { return !clients_.empty(); });

    auto client = std::move(clients_.front());
    clients_.pop();

    std::weak_ptr<RedisPool> self_weak = shared_from_this();
    LOG_INFO("[RedisPool] Client checked out (remaining: {})", clients_.size());
    return std::shared_ptr<RedisClient>(client.release(), [self_weak](RedisClient* ptr) {
        if (auto self = self_weak.lock()) {
            self->Release(ptr);
        } else {
            delete ptr;  // pool 已销毁，直接释放
        }
    });
}

void RedisPool::Release(RedisClient* client) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!client->IsConnected()) {
        LOG_WARN("[RedisPool] Reconnecting stale RedisClient...");
        client->Connect();
    }
    clients_.push(std::unique_ptr<RedisClient>(client));
    size_t available = clients_.size();
    lock.unlock();
    cond_.notify_one();
    LOG_INFO("[RedisPool] Client released back (available: {})", available);
}

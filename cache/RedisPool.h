#pragma once
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "RedisClient.h"

class RedisPool : public std::enable_shared_from_this<RedisPool> {
public:
    RedisPool(const std::string& host, int port, size_t pool_size, const std::string& password = "", int timeout_ms = 1000);

    ~RedisPool();

    std::shared_ptr<RedisClient> GetClient();

private:
    void Release(RedisClient* client);

private:
    std::string host_;
    int port_;
    std::string password_;
    struct timeval timeout_;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<std::unique_ptr<RedisClient>> clients_;
};

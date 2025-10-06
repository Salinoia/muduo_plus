#pragma once

#include <hiredis/hiredis.h>

#include <string>

// -----------------------------------------------------------------------------
// RedisClient
// -----------------------------------------------------------------------------
// 单连接 Redis 客户端封装：提供 Connect/Get/Set/Del 等同步操作。
// 线程不安全，需配合连接池（RedisPool）使用。
// -----------------------------------------------------------------------------
class RedisClient {
public:
    RedisClient(const std::string& host, int port, const std::string& password = "", const struct timeval& timeout = {1, 500000});
    ~RedisClient() noexcept;

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;
    RedisClient(RedisClient&&) noexcept = default;
    RedisClient& operator=(RedisClient&&) noexcept = default;

    bool Connect();
    void Close() noexcept;
    bool IsConnected() const noexcept;

    bool Get(const std::string& key, std::string& value);
    bool Set(const std::string& key, const std::string& value);
    bool Del(const std::string& key);

private:
    bool EnsureConnected();

    std::string host_;
    int port_;
    std::string password_;
    struct timeval timeout_;
    redisContext* context_;
};

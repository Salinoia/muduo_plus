#include "RedisClient.h"

#include <iostream>

// 构造函数
RedisClient::RedisClient(const std::string& host, int port, const std::string& password, const struct timeval& timeout) :
    host_(host), port_(port), password_(password), timeout_(timeout), context_(nullptr) {}

// 析构函数
RedisClient::~RedisClient() noexcept {
    Close();
}

// 连接
bool RedisClient::Connect() {
    Close();
    context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout_);
    if (!context_ || context_->err) {
        if (context_) {
            std::cerr << "[RedisClient] Connect error: " << context_->errstr << std::endl;
            redisFree(context_);
        }
        context_ = nullptr;
        return false;
    }

    if (!password_.empty()) {
        redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(context_, "AUTH %s", password_.c_str()));
        bool ok = reply && reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK";
        if (reply)
            freeReplyObject(reply);
        if (!ok) {
            std::cerr << "[RedisClient] Auth failed\n";
            Close();
            return false;
        }
    }
    return true;
}

// 关闭连接
void RedisClient::Close() noexcept {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
}

// 连接状态
bool RedisClient::IsConnected() const noexcept {
    return context_ && !context_->err;
}

// 确保连接可用
bool RedisClient::EnsureConnected() {
    if (!IsConnected())
        return Connect();
    return true;
}

// GET
bool RedisClient::Get(const std::string& key, std::string& value) {
    if (!EnsureConnected())
        return false;
    redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(context_, "GET %s", key.c_str()));
    if (!reply)
        return false;
    bool ok = (reply->type == REDIS_REPLY_STRING);
    if (ok)
        value.assign(reply->str, reply->len);
    freeReplyObject(reply);
    return ok;
}

// SET
bool RedisClient::Set(const std::string& key, const std::string& value) {
    if (!EnsureConnected())
        return false;
    redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK";
    if (reply)
        freeReplyObject(reply);
    return ok;
}

// DEL
bool RedisClient::Del(const std::string& key) {
    if (!EnsureConnected())
        return false;
    redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(context_, "DEL %s", key.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    if (reply)
        freeReplyObject(reply);
    return ok;
}

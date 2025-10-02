#include "RedisClient.h"

#include <iostream>
#include <strings.h>

RedisClient::RedisClient(const std::string& host, int port,
                         const std::string& password,
                         const struct timeval& timeout)
    : context_(nullptr), host_(host), port_(port), password_(password), timeout_(timeout) {}

RedisClient::~RedisClient() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
}

bool RedisClient::Connect() {
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }

    context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout_);
    if (!context_ || context_->err) {
        if (context_) {
            std::cerr << "Redis connect error: " << context_->errstr << std::endl;
            redisFree(context_);
            context_ = nullptr;
        } else {
            std::cerr << "Redis connect error." << std::endl;
        }
        return false;
    }
    redisSetTimeout(context_, timeout_);
    if (!password_.empty()) {
        redisReply* reply = (redisReply*)redisCommand(context_, "AUTH %s", password_.c_str());
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            if (reply) {
                std::cerr << "Redis auth error: " << reply->str << std::endl;
                freeReplyObject(reply);
            }
            redisFree(context_);
            context_ = nullptr;
            return false;
        }
        freeReplyObject(reply);
    }
    return true;
}

bool RedisClient::EnsureConnected() {
    if (context_ && context_->err == 0) {
        return true;
    }
    return Connect();
}

bool RedisClient::Get(const std::string& key, std::string& value) {
    if (!EnsureConnected()) return false;
    redisReply* reply = (redisReply*)redisCommand(context_, "GET %s", key.c_str());
    if (!reply) {
        std::cerr << "Redis GET command failed" << std::endl;
        return false;
    }
    bool ok = false;
    if (reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    if (!EnsureConnected()) return false;
    redisReply* reply = (redisReply*)redisCommand(context_, "SET %s %s", key.c_str(), value.c_str());
    if (!reply) {
        std::cerr << "Redis SET command failed" << std::endl;
        return false;
    }
    bool ok = reply->type == REDIS_REPLY_STATUS && strcasecmp(reply->str, "OK") == 0;
    freeReplyObject(reply);
    return ok;
}

bool RedisClient::Del(const std::string& key) {
    if (!EnsureConnected()) return false;
    redisReply* reply = (redisReply*)redisCommand(context_, "DEL %s", key.c_str());
    if (!reply) {
        std::cerr << "Redis DEL command failed" << std::endl;
        return false;
    }
    bool ok = reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    freeReplyObject(reply);
    return ok;
}


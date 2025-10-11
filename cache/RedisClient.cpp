#include "RedisClient.h"
#include "LogMacros.h"

RedisClient::RedisClient(const std::string& host, int port, const std::string& password, const struct timeval& timeout) :
    host_(host), port_(port), password_(password), timeout_(timeout), context_(nullptr) {}

RedisClient::~RedisClient() noexcept {
    Close();
}

bool RedisClient::Connect() {
    Close();
    context_ = redisConnectWithTimeout(host_.c_str(), port_, timeout_);
    if (!context_ || context_->err) {
        if (context_) {
            LOG_ERROR("[RedisClient] Connect error: {}", context_->errstr);
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
            LOG_ERROR("[RedisClient] Auth failed for {}:{}", host_, port_);
            Close();
            return false;
        }
    }

    LOG_INFO("[RedisClient] Connected successfully to {}:{}", host_, port_);
    return true;
}

void RedisClient::Close() noexcept {
    if (context_) {
        LOG_INFO("[RedisClient] Closing connection to {}:{}", host_, port_);
        redisFree(context_);
        context_ = nullptr;
    }
}

bool RedisClient::IsConnected() const noexcept {
    return context_ && !context_->err;
}

bool RedisClient::EnsureConnected() {
    if (!IsConnected())
        return Connect();
    return true;
}

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

bool RedisClient::Set(const std::string& key, const std::string& value) {
    if (!EnsureConnected())
        return false;
    redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK";
    if (reply)
        freeReplyObject(reply);
    return ok;
}

bool RedisClient::Del(const std::string& key) {
    if (!EnsureConnected())
        return false;
    redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(context_, "DEL %s", key.c_str()));
    bool ok = reply && reply->type == REDIS_REPLY_INTEGER && reply->integer > 0;
    if (reply)
        freeReplyObject(reply);
    return ok;
}

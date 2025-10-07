#include "SessionManager.h"

#include <openssl/rand.h>

#include <chrono>
#include <iomanip>
#include <sstream>

#include "LogMacros.h"
#include "Session.h"

SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage) : storage_(std::move(storage)), rng_(rd_()) {}

// 从请求中获取或创建会话
std::shared_ptr<Session> SessionManager::getSession(const HttpRequest& req, HttpResponse* resp) {
    std::string sid = getSessionIdFromCookie(req);
    auto session = storage_->load(sid);

    if (!session || session->isExpired()) {
        // 创建新会话
        std::string newId = generateSessionId();
        session = std::make_shared<Session>(newId, weak_from_this());
        storage_->save(session);
        setSessionCookie(newId, resp);
        LOG_DEBUG("Created new session: {}", newId);
    } else {
        // 刷新存活时间
        session->refresh();
        storage_->save(session);
    }

    return session;
}
// 销毁会话
void SessionManager::destroySession(const std::string& sessionId) {
    storage_->remove(sessionId);
}

// 清理过期会话
void SessionManager::cleanExpiredSessions() {
    storage_->clearExpired();
}

// 生成高熵 Session ID
std::string SessionManager::generateSessionId() {
    unsigned char bytes[16];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        // 备用：使用 std::mt19937_64
        std::scoped_lock lock(mutex_);
        std::uniform_int_distribution<uint64_t> dist;
        std::ostringstream oss;
        oss << std::hex << dist(rng_) << dist(rng_);
        return oss.str();
    }

    std::ostringstream oss;
    for (auto b : bytes)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int) b;
    return oss.str();
}

// 从 Cookie 获取 SessionId
std::string SessionManager::getSessionIdFromCookie(const HttpRequest& req) {
    auto cookieHeader = req.getHeader("Cookie");
    if (cookieHeader.empty())
        return {};

    static const std::string prefix = "SESSIONID=";
    auto pos = cookieHeader.find(prefix);
    if (pos == std::string::npos)
        return {};

    pos += prefix.size();
    auto end = cookieHeader.find(';', pos);
    if (end == std::string::npos)
        end = cookieHeader.size();
    return cookieHeader.substr(pos, end - pos);
}

// 在响应中设置 Cookie
void SessionManager::setSessionCookie(const std::string& sessionId, HttpResponse* resp) {
    std::ostringstream cookie;
    cookie << "SESSIONID=" << sessionId << "; Path=/; HttpOnly; SameSite=Lax; Max-Age=3600";
    resp->addHeader("Set-Cookie", cookie.str());
}

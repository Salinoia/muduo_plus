#pragma once

#include <memory>
#include <random>
#include <string>
#include <mutex>

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "SessionStorage.h"
#include "Session.h"

/**
 * @brief 会话管理器（SessionManager）
 * 
 * 负责：
 * - 从 Cookie 读取 SessionId；
 * - 加载 / 创建 / 保存 Session；
 * - 管理过期清理；
 * 
 * 与 Session 互相弱引用，防止循环依赖。
 */
class SessionManager : public std::enable_shared_from_this<SessionManager> {
public:
    explicit SessionManager(std::unique_ptr<SessionStorage> storage);

    // 从请求中获取或创建会话
    std::shared_ptr<Session> getSession(const HttpRequest& req, HttpResponse* resp);

    // 销毁指定会话
    void destroySession(const std::string& sessionId);

    // 清理过期会话（由定时任务调用）
    void cleanExpiredSessions();

    // 持久化更新
    void updateSession(std::shared_ptr<Session> session) { storage_->save(std::move(session)); }


private:
    // 生成高熵 Session ID
    std::string generateSessionId();

    // 从 Cookie 中提取 SessionId
    std::string getSessionIdFromCookie(const HttpRequest& req);

    // 设置响应 Cookie
    void setSessionCookie(const std::string& sessionId, HttpResponse* resp);

private:
    std::unique_ptr<SessionStorage> storage_;
    std::random_device rd_;
    std::mt19937_64 rng_;
    std::mutex mutex_; // 保护随机生成或共享资源
};


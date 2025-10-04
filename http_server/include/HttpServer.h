#pragma once

#include <functional>
#include <memory>
#include <string>

// 连接层模块
#include "EventLoop.h"
#include "InetAddress.h"
#include "NonCopyable.h"
#include "TcpConnection.h"
#include "TcpServer.h"

// 应用层模块
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Middleware.h"
#include "MiddlewareChain.h"
#include "Router.h"
#include "SessionManager.h"
#include "TLSConnection.h"
#include "TLSContext.h"

class HttpServer : public NonCopyable {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    // 由外部注入 EventLoop（避免自持 mainLoop_ 带来的耦合）
    HttpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name, bool useTLS = false, TcpServer::Option option = TcpServer::kNoReusePort);

    // 线程配置/启动
    void setThreadNum(int n) { server_.setThreadNum(n); }
    void start();

    // 业务回调（兜底）
    void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }

    // 路由（静态）
    void Get(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kGet, path, cb); }
    void Post(const std::string& path, const HttpCallback& cb) { router_.registerCallback(HttpRequest::kPost, path, cb); }
    void Get(const std::string& path, Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kGet, path, handler); }
    void Post(const std::string& path, Router::HandlerPtr handler) { router_.registerHandler(HttpRequest::kPost, path, handler); }

    // 路由（正则/动态）
    void addRoute(HttpRequest::Method m, const std::string& path, Router::HandlerPtr h) { router_.addRegexHandler(m, path, h); }

    void addRoute(HttpRequest::Method m, const std::string& path, const Router::HandlerCallback& cb) { router_.addRegexCallback(m, path, cb); }

    // 会话 & 中间件
    void setSessionManager(std::unique_ptr<SessionManager> m) { sessionMgr_ = std::move(m); }
    SessionManager* sessionManager() const { return sessionMgr_.get(); }
    void addMiddleware(std::shared_ptr<Middleware> m) { middlewares_.addMiddleware(std::move(m)); }


    // TLS 开关与上下文
    void enableTLS(bool on) { useTLS_ = on; }
    void setTlsContext(std::shared_ptr<TLSContext> ctx) { tlsCtx_ = std::move(ctx); }

private:
    // —— 事件派发（统一入口）——
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts);

    // —— 明文处理（无论是否启用 TLS，最终都走这里）——
    void onPlainMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts);

    void handleHttpRequest(const TcpConnectionPtr& conn, HttpRequest& req);

    // —— TLS 相关（与连接强绑定；通过 TcpConnection::setContext 存放）——
    using TlsConnPtr = std::shared_ptr<TLSConnection>;
    static TlsConnPtr getTls(const TcpConnectionPtr& c);

private:
    TcpServer server_;
    Router router_;
    MiddlewareChain middlewares_;
    std::shared_ptr<Session> session_;
    std::unique_ptr<SessionManager> sessionMgr_;

    HttpCallback httpCallback_;  // 兜底业务回调
    bool useTLS_{false};
    std::shared_ptr<TLSContext> tlsCtx_;  // 线程安全共享
    

    // 禁止默认构造
    HttpServer() = delete;
};

#include "HttpServer.h"

#include "Buffer.h"
#include "HttpContext.h"
#include "LogMacros.h"

using namespace std;

// ==========================
//  http::HttpServer 实现
// ==========================

HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr, const string& name, bool useTLS, TcpServer::Option option) : server_(loop, listenAddr, name, option), useTLS_(useTLS) {
    // 注册连接与消息回调
    server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });

    server_.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts) { onMessage(conn, buf, ts); });
}

void HttpServer::start() {
    if (useTLS_ && !tlsCtx_) {
        LOG_FATAL("TLS enabled but no TLSContext provided");
    }
    server_.start();
}

void HttpServer::stop() {
    LOG_INFO("[HttpServer] Stopping server...");
    server_.stop();
    LOG_INFO("[HttpServer] Shutdown complete");
}


void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        if (useTLS_) {
            // 为每条连接创建独立 TLSConnection
            auto tls = std::make_shared<TLSConnection>(conn, tlsCtx_.get());

            // 设置解密后回调路径
            tls->setMessageCallback([this](const TcpConnectionPtr& c, Buffer* b, Timestamp t) { onPlainMessage(c, b, t); });

            conn->setContext(tls);
            tls->startHandshake();
        }
    } else {
        // 连接关闭时释放上下文
        if (conn->getContext().has_value()) {
            try {
                auto tls = std::any_cast<TlsConnPtr>(conn->getContext());
                tls.reset();
            } catch (const std::bad_any_cast&) {
                // ignore
            }
            conn->setContext(std::any());
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts) {
    if (useTLS_) {
        try {
            if (conn->getContext().has_value()) {
                auto tls = std::any_cast<TlsConnPtr>(conn->getContext());
                if (tls) {
                    tls->onRead(conn, buf, ts);
                    return;  // 解密后的数据将回调 onPlainMessage
                }
            }
        } catch (const std::bad_any_cast&) {
            // 继续走明文路径
        }
    }
    onPlainMessage(conn, buf, ts);
}

void HttpServer::onPlainMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts) {
    // 每个连接维护自己的 HttpContext
    HttpContext* context = nullptr;
    if (!conn->getContext().has_value() || conn->getContext().type() != typeid(HttpContext)) {
        conn->setContext(HttpContext());
    }

    try {
        context = std::any_cast<HttpContext>(&conn->getMutableContext());
    } catch (const std::bad_any_cast&) {
        conn->setContext(HttpContext());
        context = std::any_cast<HttpContext>(&conn->getMutableContext());
    }

    if (!context->parseRequest(buf, ts)) {
        HttpResponse resp(false);
        resp.setStatusCode(HttpResponse::k400BadRequest);
        resp.setStatusMessage("Bad Request");
        Buffer out;
        resp.appendToBuffer(&out);
        conn->send(&out);
        conn->shutdown();
        return;
    }

    if (context->gotAll()) {
        handleHttpRequest(conn, context->request());
        context->reset();  // 为下一次请求复用
    }
}

void HttpServer::handleHttpRequest(const TcpConnectionPtr& conn, HttpRequest& req) {
    HttpResponse resp(req.getHeader("Connection") == "close");

    // 会话管理（若启用）
    if (sessionMgr_) {
        sessionMgr_->getSession(req, &resp);
    }

    // 中间件链执行
    bool cont = middlewares_.handle(req, resp);
    if (!cont) {
        Buffer out;
        resp.appendToBuffer(&out);
        conn->send(&out);
        if (resp.closeConnection())
            conn->shutdown();
        return;
    }

    // 路由匹配
    bool handled = router_.route(req, &resp);
    if (!handled && httpCallback_) {
        httpCallback_(req, &resp);
        handled = true;
    }

    // 404 兜底
    if (!handled) {
        resp.setStatusCode(HttpResponse::k404NotFound);
        resp.setStatusMessage("Not Found");
        resp.setContentType("text/plain; charset=utf-8");
        resp.setBody("404 Not Found");
    }

    Buffer out;
    resp.appendToBuffer(&out);
    conn->send(&out);
    if (resp.closeConnection())
        conn->shutdown();
}

HttpServer::TlsConnPtr HttpServer::getTls(const TcpConnectionPtr& c) {
    if (!c->getContext().has_value())
        return nullptr;
    try {
        return std::any_cast<TlsConnPtr>(c->getContext());
    } catch (const std::bad_any_cast&) {
        return nullptr;
    }
}

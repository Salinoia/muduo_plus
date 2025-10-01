#include "http/HttpServer.h"

#include "Buffer.h"
#include "Logger.h"

HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name, TcpServer::Option option) : server_(loop, listenAddr, name, option) {
    server_.setConnectionCallback([this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback([this](const TcpConnectionPtr& conn, Buffer* buf, TimeStamp time) { onMessage(conn, buf, time); });
}

void HttpServer::start() {
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        contexts_.emplace(conn.get(), HttpContext());
    } else {
        contexts_.erase(conn.get());
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp receiveTime) {
    auto it = contexts_.find(conn.get());
    if (it == contexts_.end()) {
        HttpContext ctx;
        it = contexts_.emplace(conn.get(), std::move(ctx)).first;
    }
    HttpContext& context = it->second;
    if (!context.parseRequest(buf, receiveTime)) {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
    if (context.gotAll()) {
        onRequest(conn, context.request());
        context.reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, HttpRequest& req) {
    bool close = false;
    if (req.getHeader("Connection") == "close" || (req.version() == HttpRequest::kHttp10 && req.getHeader("Connection") != "Keep-Alive")) {
        close = true;
    }
    HttpResponse response(close);
    if (httpCallback_) {
        httpCallback_(req, response);
    } else {
        response.setStatusCode(HttpResponse::k404NotFound);
        response.setStatusMessage("Not Found");
        response.setCloseConnection(true);
    }
    Buffer buf;
    response.appendToBuffer(&buf);
    conn->send(buf.retrieveAllAsString());
    if (response.closeConnection()) {
        conn->shutdown();
    }
}

#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "TcpServer.h"
#include "http/HttpContext.h"
#include "http/HttpResponse.h"

class HttpServer {
public:
    using HttpCallback = std::function<void(HttpRequest&, HttpResponse&)>;

    HttpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name, TcpServer::Option option = TcpServer::kNoReusePort);

    void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }

    void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
    void start();

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, TimeStamp receiveTime);
    void onRequest(const TcpConnectionPtr& conn, HttpRequest& req);

    TcpServer server_;
    HttpCallback httpCallback_;
    std::unordered_map<TcpConnection*, HttpContext> contexts_;
};

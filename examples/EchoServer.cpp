#include "Logger.h"
#include "TcpServer.h"

class EchoServer : public std::enable_shared_from_this<EchoServer> {
public:
    using Ptr = std::shared_ptr<EchoServer>;

    static Ptr create(EventLoop* loop, const InetAddress& addr, const std::string& name) { return std::make_shared<EchoServer>(loop, addr, name); }
    EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name) : loop_(loop), server_(loop, addr, name) {}
    ~EchoServer() = default;
    void start() {
        auto self = shared_from_this();
        server_.setConnectionCallback([self](const TcpConnectionPtr& conn) { self->onConnection(conn); });

        server_.setMessageCallback([self](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) { self->onMessage(conn, buf, time); });

        server_.setThreadNum(4);
        server_.start();
    }

private:
    EventLoop* loop_;
    TcpServer server_;
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            Logger::instance().info("Connection Up : %s", conn->peerAddress().toIpPort().c_str());
        } else {
            Logger::instance().info("Connection closed [%s -> %s]", conn->localAddress().toIpPort().c_str(), conn->peerAddress().toIpPort().c_str());
        }
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
        std::string request = buf->retrieveAllAsString();
        // 简易HTTP协议识别
        if (request.find("GET / HTTP/1.") != std::string::npos) {
            std::string response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Length: 13\r\n"
                                   "\r\n"
                                   "Hello, World!";
            conn->send(response);
        }
        // 保持原有TCP回显逻辑
        else {
            conn->send(request);
        }
    }
};

int main() {
    Logger::instance().setLogLevel(LogLevel::INFO);
    EventLoop loop;
    InetAddress addr("127.0.0.1", 8000);
    auto server = EchoServer::create(&loop, addr, "EchoServer");
    server->start();
    loop.loop();
    return 0;
}
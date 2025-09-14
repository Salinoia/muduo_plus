#include "TcpServer.h"

#include <string.h>

#include <functional>

#include "Logger.h"
#include "TcpConnection.h"

namespace {
EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
}  // namespace

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option) :
    loop_(CheckLoopNotNull(loop)),
    ipPort_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop, name_)),
    nextConnId_(1),
    started_(0),
    connectionCallback_(),
    messageCallback_() {  
    // 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，执行handleRead()调用TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& listenAddr) { this->newConnection(sockfd, listenAddr); });
}

TcpServer::~TcpServer() {
    for (auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();  // 复位原始的智能指针，将栈空间的TcpConnectionPtr conn指向该对象，超出作用域即可释放
        conn->getLoop()->runInLoop([conn]() { conn->connectDestroyed(); });  // 销毁连接
    }
}

// 设置subloop的个数
void TcpServer::setThreadNum(int numThreads) {
    int numThreads_ = numThreads;
    threadPool_->setThreadNum(numThreads_);
}

// 开启服务器监听
void TcpServer::start() {
    if (started_.fetch_add(1) == 0) {  // 防止一个TcpServer对象被start多次
        threadPool_->start(threadInitCallback_);  // 启动底层的loop线程池
        loop_->runInLoop([this] {  // 依赖TcpServer对象保持存活
            acceptor_->listen();
        });
    }
}

// 每当有新用户连接时，acceptor会执行回调操作
// 将mainLoop接收到的强求连接通过回调轮询分发给subLoop
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    // 轮询算法：选择一个subLoop来管理connfd对应的channel
    EventLoop* ioLoop = threadPool_->getNextLoop();

    // ++nextConnId_;  // 没有设置为原子类是因为其只在mainloop中执行，不存在线程安全问题
    int connId = nextConnId_.fetch_add(1, std::memory_order_relaxed); // 即使如此依然需要全部采取原子操作保持一致性
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), connId); // 注意：通过load()方法获取原子变量的值
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n", name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    sockaddr_in local;
    socklen_t addrLen = sizeof(local);
    ::memset(&local, 0, addrLen);
    if (::getsockname(sockfd, (sockaddr*) &local, &addrLen) < 0) {
        LOG_ERROR("socket::getLocalAddr");
    }
    InetAddress localAddr(local);
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;

    // 设置回调函数：TcpServer => TcpConnection
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置关闭连接的回调
    // conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    conn->setCloseCallback([this](const TcpConnectionPtr& conn) { removeConnection(conn); });
    // ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
    ioLoop->runInLoop([conn]() { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    // loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
    loop_->runInLoop([this, conn]() { this->removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    // ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
}
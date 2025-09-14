#include "Acceptor.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "InetAddress.h"
#include "Logger.h"

static int createNonBlockingSocket() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort) :
    loop_(loop),  // 初始化事件循环指针
    acceptSocket_(createNonBlockingSocket()),  // 创建非阻塞监听套接字
    acceptChannel_(loop, acceptSocket_.getSocketFd()),  // 创建监听通道
    listenning_(false)  // 初始状态未开始监听
{
    acceptSocket_.setReuseAddr(true);  // 设置SO_REUSEADDR选项（快速重启）
    acceptSocket_.setReusePort(reusePort);  // 设置SO_REUSEPORT选项（多线程监听同一端口）
    acceptSocket_.bindAddress(listenAddr);  // 绑定监听地址（IP+Port）
    // TcpServer::start() => Acceptor.listen() 如果有新用户连接 要执行一个回调(accept => connfd => 打包成Channel => 唤醒subloop)
    // baseloop监听到有事件发生 => acceptChannel_(listenfd) => 执行该回调函数
    acceptChannel_.setReadCallback([this](TimeStamp t) { this->handleRead(); });
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();  // 把从Poller中感兴趣的事件删除掉
    acceptChannel_.remove();  // 调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
}

void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();  // 核心操作：将acceptChannel_注册到Poller
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (NewConnectionCallback_) {
            NewConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        LOG_ERROR("%s:%s:%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        if (errno == EMFILE) {
            LOG_ERROR("%s:%s:%d sockfd reached limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
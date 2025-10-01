#include "TcpConnection.h"

#include <sys/sendfile.h> // for sendfile

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null!\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}
TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr) :
    loop_(CheckLoopNotNull(loop)),
    name_(nameArg),
    state_(kConnecting),
    reading_(true),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64 * 1024 * 1024)  // 64M
{
    channel_->setReadCallback([this](TimeStamp t) { this->handleRead(t); });
    channel_->setWriteCallback([this]() { this->handleWrite(); });
    channel_->setCloseCallback([this]() { this->handleClose(); });
    channel_->setErrorCallback([this]() { this->handleError(); });
    LOG_TRACE("TcpConnection::ctor[%s] at fd = %d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO("TcpConnection::dtor[%s] at fd = %d state = %d\n", name_.c_str(), channel_->getFd(), (int) state_);
}

void TcpConnection::send(const std::string& buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {  // 对于单个reactor的情况 用户调用conn->send时 loop_即为当前线程
            sendInLoop(buf.c_str(), buf.size());
        } else {
            loop_->runInLoop([this, buf]() {
                if (state_ == kConnected) {  // 再次检查状态，因为状态可能在排队时改变
                    sendInLoop(buf.c_str(), buf.size());
                }
            });
        }
    }
}

void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count) {
    if (connected()) {
        if (loop_->isInLoopThread()) {  // 是否位于当前循环
            sendFileInLoop(fileDescriptor, offset, count);
        } else {  // 如果不是，则唤醒运行这个TcpConnection的线程执行Loop循环
            loop_->runInLoop([self = shared_from_this(), fileDescriptor, offset, count]() { self->sendFileInLoop(fileDescriptor, offset, count); });
        }
    } else {
        LOG_ERROR("TcpConnection::sendFile : not connected");
    }
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop([this]() { this->shutdownInLoop(); });
    }
}

void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的EPOLLIN读事件
    // 新连接建立 执行回调
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());  // 将 channel 中的事件从 poller 中删除
    }
    channel_->remove();  // 将 channel 从 poller 中删除
}

// 读是相对服务器而言的 当对端客户端有数据到达 服务器端检测到 EPOLL_IN 就会触发该fd上的回调 handleRead取读走对端发来的数据
void TcpConnection::handleRead(TimeStamp receiveTime) {
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &saveErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite() {
    if (channel_->isWriting()) {
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->getFd(), &saveErrno);
        if (n > 0) {
            outputBuffer_.retrieve(n);  // 从缓冲区读取 reabable 区域数据移动到 readIndex 下标
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    loop_->queueInLoop([self = shared_from_this()] { self->writeCompleteCallback_(self); });
                }
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    } else {
        LOG_ERROR("TcpConnection fd = %d is down, no more writing operations", channel_->getFd());
    }
}

void TcpConnection::handleClose() {
    LOG_INFO("TcpConnection::handleClose fd = %d state = %d\n", channel_->getFd(), (int) state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);  // 连接回调
    closeCallback_(connPtr);  // 执行关闭连接的回调 执行的是 TcpServer::removeConnection 回调方法，必须最后执行
}

void TcpConnection::handleError() {
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
        err = errno;
    } else {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name: %s - SO_ERROR: %d\n", name_.c_str(), err);
}

void TcpConnection::sendInLoop(const void* data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected) {
        LOG_ERROR("Disconnected, give up writing operations.");
        // return;
    }
    // 第一次开始写数据或缓冲区没有带发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        // nwrote = ::write(channel_->getFd(), data, len); // 如果对端关闭连接，此处调用write()会触发SIGPIPE,默认终止程序
        nwrote = ::send(channel_->getFd(), data, len, MSG_NOSIGNAL);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            auto self = shared_from_this();
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop([self]() {
                    if (self->writeCompleteCallback_)
                        self->writeCompleteCallback_(self);
                });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {  // EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
                LOG_ERROR("TcpConnection::sendInLoop write error");
                if (errno == EPIPE || errno == ECONNRESET) {  // SIGPIPE RESET
                    faultError = true;
                }
            }
        }
    }
    // 说明当前这一次write并没有把数据全部发送出去 剩余的数据需要保存到缓冲区当中
    if (!faultError && remaining > 0) {
        ssize_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining > oldLen) {
            if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
                auto self = shared_from_this();
                loop_->queueInLoop([self, oldLen, remaining] {
                    self->highWaterMarkCallback_(self, oldLen + remaining);
                });
            }
            outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        }
        if (!channel_->isWriting()) { // 这里需要注册channel的写事件 否则poller不会给channel通知epollout
            channel_->enableWriting();
        }
    }

    if (faultError) {
        handleClose(); // 主动关闭连接
    }
}

void TcpConnection::shutdownInLoop() {
    if (!channel_->isWriting()){
        socket_->shutdownWrite();
    }
}

void TcpConnection::sendFileInLoop(int fileDescriptor, off_t offset, size_t count) {
    ssize_t bytesSent = 0; // 发送了多少字节数
    size_t remaining = count; // 还要多少数据要发送
    bool faultError = false; // 错误的标志位

    if (state_ == kDisconnecting) { // 表示此时连接已经断开就不需要发送数据了
        LOG_ERROR("disconnected, give up writing");
        return;
    }
    // 表示Channel第一次开始写数据或者outputBuffer缓冲区中没有数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        bytesSent = sendfile(socket_->getSocketFd(), fileDescriptor, &offset, remaining);
        if (bytesSent >= 0) {
            remaining -= bytesSent;
            if (remaining == 0 && writeCompleteCallback_) {
                // remaining为0意味着数据正好全部发送完，就不需要给其设置写事件的监听。
                auto self = shared_from_this();
                loop_->queueInLoop([self]() {
                    self->writeCompleteCallback_(self);
                });
            }
        } else {  // bytesSent < 0
            if (errno != EWOULDBLOCK) {  // 如果是非阻塞没有数据返回错误这个是正常显现等同于EAGAIN，否则就异常情况
                LOG_ERROR("TcpConnection::sendFileInLoop");
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                faultError = true;
            }
        }
    }
    // 处理剩余数据
    if (!faultError && remaining > 0) {
        // 继续发送剩余数据
        auto self = shared_from_this();
        loop_->queueInLoop([self, fileDescriptor, offset, remaining]() {
            self->sendFileInLoop(fileDescriptor, offset, remaining);
        });
    }
}

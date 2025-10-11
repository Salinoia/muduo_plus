#include "TcpConnection.h"

#include <errno.h>
#include <sys/sendfile.h>  // for sendfile

#include "Channel.h"
#include "EventLoop.h"
#include "LogMacros.h"
#include "Socket.h"

struct PendingFile {
    int    fd = -1;
    off_t  offset = 0;
    size_t remaining = 0;
    bool   active = false;
};
PendingFile pendingFile_;
static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("mainLoop is null!");
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr) :
    loop_(CheckLoopNotNull(loop)),
    state_(kConnecting),
    reading_(true),
    socket_(std::make_unique<Socket>(sockfd)),
    channel_(std::make_unique<Channel>(loop, sockfd)),
    name_(nameArg),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64 * 1024 * 1024)  // 64MB
{
    channel_->setReadCallback([this](Timestamp t) { handleRead(t); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });

    socket_->setKeepAlive(true);
    LOG_TRACE("TcpConnection::ctor [{}] fd = {}", name_, sockfd);
}

TcpConnection::~TcpConnection() {
    LOG_INFO("TcpConnection::dtor [{}] fd = {} state = {}", name_, channel_->getFd(), state_.load());
}

// ===================== send 系列 =====================

void TcpConnection::send(const std::string& buf) {
    send(buf.data(), buf.size());
}

void TcpConnection::send(const void* data, size_t len) {
    if (state_ != kConnected)
        return;

    if (loop_->isInLoopThread()) {
        sendInLoop(data, len);
    } else {
        // 拷贝数据，防止原缓冲释放
        std::string copy(static_cast<const char*>(data), len);
        loop_->runInLoop([self = shared_from_this(), copy]() {
            if (self->state_ == kConnected) {
                self->sendInLoop(copy.data(), copy.size());
            }
        });
    }
}

// 上层调用的 Buffer* 版本
void TcpConnection::send(Buffer* buf) {
    if (!buf || buf->readableBytes() == 0)
        return;
    if (state_ != kConnected)
        return;  // 阻止误清空

    if (loop_->isInLoopThread()) {
        sendInLoop(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
    } else {
        std::string copy(buf->peek(), buf->readableBytes());
        buf->retrieveAll();
        auto self = shared_from_this();
        loop_->runInLoop([self, copy = std::move(copy)]() {
            if (self->state_ == kConnected)
                self->sendInLoop(copy.data(), copy.size());
        });
    }
}

void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count) {
    if (state_ != kConnected)
        return;

    if (loop_->isInLoopThread()) {
        sendFileInLoop(fileDescriptor, offset, count);
    } else {
        loop_->runInLoop([self = shared_from_this(), fileDescriptor, offset, count]() {
            if (self->state_ == kConnected)
                self->sendFileInLoop(fileDescriptor, offset, count);
        });
    }
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop([self = shared_from_this()] { self->shutdownInLoop(); });
    }
}

// ===================== 生命周期管理 =====================

void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 注册EPOLLIN事件
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
    LOG_INFO("connectDestroyed() fd={} state={}", channel_->getFd(), state_.load());
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();
    }
    channel_->remove();
}

// ===================== 事件回调 =====================

void TcpConnection::handleRead(Timestamp receiveTime) {
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &saveErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead() errno = {}", errno);
        handleError();
    }
}
void TcpConnection::handleWrite() {
    // 先处理待续传文件
    if (pendingFile_.active) {
        ssize_t n = ::sendfile(socket_->getSocketFd(), pendingFile_.fd, &pendingFile_.offset, pendingFile_.remaining);
        if (n >= 0) {
            pendingFile_.remaining -= static_cast<size_t>(n);
            if (pendingFile_.remaining == 0) {
                pendingFile_.active = false;
                // 文件发完后，继续走缓冲区逻辑
                if (writeCompleteCallback_ && outputBuffer_.readableBytes() == 0) {
                    auto self = shared_from_this();
                    loop_->queueInLoop([self] {
                        if (self->writeCompleteCallback_)
                            self->writeCompleteCallback_(self);
                    });
                }
            } else {
                channel_->enableWriting();  // 继续等待下一次可写
                return;
            }
        } else {
            if (errno == EWOULDBLOCK) {
                channel_->enableWriting();
                return;
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                handleClose();
                return;
            }
            LOG_ERROR("TcpConnection::handleWrite sendfile errno = {}", errno);
            pendingFile_.active = false;  // 降级：停止续传
        }
    }

    // 再处理内存缓冲
    if (!channel_->isWriting()) {
        LOG_WARN("handleWrite called but not writing fd = {}", channel_->getFd());
        return;
    }
    int saveErrno = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->getFd(), &saveErrno);
    if (n > 0) {
        outputBuffer_.retrieve(n);
        if (outputBuffer_.readableBytes() == 0) {
            channel_->disableWriting();
            if (writeCompleteCallback_) {
                auto self = shared_from_this();
                loop_->queueInLoop([self] {
                    if (self->writeCompleteCallback_)
                        self->writeCompleteCallback_(self);
                });
            }
            if (state_ == kDisconnecting) {
                shutdownInLoop();
            }
        }
    } else {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleWrite() errno = {}", errno);
    }
}

void TcpConnection::handleClose() {
    LOG_INFO("TcpConnection::handleClose fd = {} state = {}", channel_->getFd(), state_.load());
    setState(kDisconnected);
    channel_->disableAll();

    auto self = shared_from_this();
    // 不要再次调用 connectionCallback_，防止上层重复处理
    if (closeCallback_)
        closeCallback_(self);
}

void TcpConnection::handleError() {
    int optval = 0;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
        err = errno;
    else
        err = optval;

    LOG_ERROR("TcpConnection::handleError name = {} SO_ERROR = {}", name_, err);
}

// ===================== 内部逻辑 =====================

void TcpConnection::sendInLoop(const void* data, size_t len) {
    if (state_ == kDisconnected) {
        LOG_WARN("sendInLoop on disconnected connection [{}]", name_);
        return;
    }

    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 尝试直接发送
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::send(channel_->getFd(), data, len, MSG_NOSIGNAL);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                auto self = shared_from_this();
                loop_->queueInLoop([self]() {
                    if (self->writeCompleteCallback_)
                        self->writeCompleteCallback_(self);
                });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                if (errno == EPIPE || errno == ECONNRESET)
                    faultError = true;
                LOG_ERROR("TcpConnection::sendInLoop write error: {}", errno);
            }
        }
    }

    // 若未发完，存入缓冲区等待EPOLLOUT
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            auto self = shared_from_this();
            loop_->queueInLoop([self, total = oldLen + remaining]() { self->highWaterMarkCallback_(self, total); });
        }

        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }

    if (faultError)
        handleClose();
}

void TcpConnection::shutdownInLoop() {
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::sendFileInLoop(int fileDescriptor, off_t offset, size_t count) {
    if (state_ != kConnected)
        return;

    // 若缓冲里还有待发数据，优先让缓冲走完，再发文件
    if (outputBuffer_.readableBytes() > 0 || channel_->isWriting()) {
        pendingFile_ = {fileDescriptor, offset, count, true};
        channel_->enableWriting();
        return;
    }

    ssize_t n = ::sendfile(socket_->getSocketFd(), fileDescriptor, &offset, count);
    if (n >= 0) {
        size_t remaining = count - static_cast<size_t>(n);
        if (remaining == 0) {
            if (writeCompleteCallback_) {
                auto self = shared_from_this();
                loop_->queueInLoop([self] {
                    if (self->writeCompleteCallback_)
                        self->writeCompleteCallback_(self);
                });
            }
            return;
        }
        // 未发完：注册写事件，后续在 handleWrite() 继续
        pendingFile_ = {fileDescriptor, offset, remaining, true};
        channel_->enableWriting();
        return;
    }

    if (errno == EWOULDBLOCK) {
        pendingFile_ = {fileDescriptor, offset, count, true};
        channel_->enableWriting();
        return;
    }

    if (errno == EPIPE || errno == ECONNRESET) {
        handleClose();
    } else {
        LOG_ERROR("TcpConnection::sendFileInLoop errno = {}", errno);
    }
}

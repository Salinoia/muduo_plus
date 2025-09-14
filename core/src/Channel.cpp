#include "Channel.h"

#include <sys/epoll.h>

#include "EventLoop.h"
#include "Logger.h"

// 静态常量在源文件中实现
const int Channel::kNoneEvent = 0;  // 空事件
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;  // 读事件
const int Channel::kWriteEvent = EPOLLOUT;  // 写事件

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}
Channel::~Channel() {}

// Channel的tie方法调用时机:TcpConnection => Channel
// TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection对象的成员方法；
// 因此Channel对象的生命周期一定长于TcpConnection对象；
// 此处用tie去解决TcpConnection和Channel的生命周期时长问题，从而保证了Channel对象能够在TcpConnection销毁前销毁；

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

// update 和 remove => EPollPoller 更新channel在poller中的状态
void Channel::update() {
    loop_->updateChannel(this);
}
void Channel::remove() {
    loop_->updateChannel(this);
}

void Channel::handleEvent(TimeStamp receiveTime) {
    // 如果Channel绑定了共享对象（如TcpConnection）
    if (tied_) {
        // 尝试提升为强引用，防止处理期间对象被销毁
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);  // 对象存活，正常处理事件
        } else {
            // 对象已销毁，无需处理（TcpConnection生命周期结束）
            LOG_ERROR("The tied object is already destroyed. Skipping event handling.");
        }
    } else {
        // 未绑定共享对象，直接处理事件
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(TimeStamp receiveTime) {
    LOG_INFO("channel handleEvent revents:%d\n", revents_);

    // 处理挂断事件（EPOLLHUP且无EPOLLIN）
    // 触发场景：对方关闭写端或连接完全关闭
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();  // 执行连接关闭回调
        }
    }

    // 处理错误事件（EPOLLERR）
    // 包括套接字错误、不可恢复错误等
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();  // 执行错误处理回调
        }
    }

    // 处理读事件（EPOLLIN或EPOLLPRI）
    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);  // 执行读回调，带时间戳
        }
    }

    // 处理写事件（EPOLLOUT）
    // 当输出缓冲区可写时触发
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();  // 执行写回调
        }
    }
}

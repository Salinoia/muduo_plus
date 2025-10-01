#include "EventLoop.h"

#include <sys/eventfd.h>

#include "Channel.h"
#include "Logger.h"
#include "Poller.h"

// 每个线程对应一个 EventLoop
thread_local EventLoop* t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTime = 10000;  // 10000ms = 10s

/**
 * 创建一个eventfd用于线程间通信，无需加锁即可同步。
 * 内核要求：Linux ≥2.6.27（2.6.26及以下需flags=0）
 *
 * @param initval  初始计数器值（此处设为0）
 * @param flags    EFD_NONBLOCK非阻塞 | EFD_CLOEXEC fork时自动关闭
 * @return         成功返回fd，失败终止进程
 *
 * 典型应用场景：
 * 1. 线程间通信（同进程）
 * 2. 父子进程间通信
 * 3. 非亲缘进程通信（需配合共享内存）
 */
int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop() :
    looping_(false),
    quit_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)) {
    LOG_DEBUG("EvnetLoop created %p in thread %d \n", this, threadId_);
    if (t_loopInThisThread == nullptr) {
        t_loopInThisThread = this;
    } else {
        LOG_FATAL("Another EventLoop %p exists in thread %d \n", t_loopInThisThread, threadId_);
    }
    wakeupChannel_->setReadCallback([this](TimeStamp t) { this->handleRead(); });  // 设置 wakeupfd 的事件类型以及发生事件后的回调操作
    wakeupChannel_->enableReading();  // 每个 EventLoop 都监听 wakeupChannel_的EPOLL读事件
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();  // 移除 Channel 中所有感兴趣的事件
    wakeupChannel_->remove();  // 将 Channel 从 EventLoop 上删除
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping\n", this);
    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTime, &activeChannels_);
        for (Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime_);  // Poller 监听事件，上报给 EventLoop通知 channel处理相应事件
        }
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping\n", this);
    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {  // 在当前线程执行回调
        cb();
    } else {  // 非当前线程执行回调，唤醒EventLoop所在线程进行回调
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::updateChannel(Channel* channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
    return poller_->hasChannel(channel);
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventLoop::handleRead() writes %lu bytes instead of 8\n", n);
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);  // 交换的方式减少了锁的临界区范围 提升效率 同时避免了死锁 如果执行functor()在临界区内 且functor()中调用queueInLoop()就会产生死锁
    }
    for (const Functor& functor : functors) {
        functor();  // 执行回调
    }
    callingPendingFunctors_ = false;
}
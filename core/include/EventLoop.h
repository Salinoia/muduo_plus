#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "CurrentThread.h"
#include "NonCopyable.h"
#include "TimeStamp.h"

class Channel;
class Poller;

// 事件循环类 主要包含了两个大模块 Channel Poller(epoll的抽象)
class EventLoop : NonCopyable {
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    TimeStamp pollReturnTime() const { return pollReturnTime_; }
    void runInLoop(Functor cb);  // 在当前loop中执行
    void queueInLoop(Functor cb);  // 把上层注册的回调函数cb放入队列中 唤醒loop所在的线程执行cb
    void wakeup();  // 通过eventfd唤醒loop对应的线程

    // EventLoop的方法 => Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己线程里：
    // threadId_为创建EventLoop对象的线程id; t_cachedTid为当前线程id；
    bool isInLoopThread() const { return CurrentThread::t_cachedTid == threadId_; }

private:
    // ==== 核心事件循环状态 ====
    std::atomic_bool looping_;  // 是否在事件循环中
    std::atomic_bool quit_;  // 是否退出循环
    const pid_t threadId_;  // 所属线程ID

    // ==== Poller 相关 ====
    using ChannelList = std::vector<Channel*>;
    std::unique_ptr<Poller> poller_;  // Poller 实例（epoll抽象）
    TimeStamp pollReturnTime_;  // Poller返回事件的时间戳
    ChannelList activeChannels_;  // 当前活跃的Channel列表

    // ==== 跨线程任务调度 ====
    int wakeupFd_;  // 用于唤醒的事件fd
    std::unique_ptr<Channel> wakeupChannel_;  // 绑定wakeupFd_的Channel
    std::vector<Functor> pendingFunctors_;  // 待执行的回调队列
    std::atomic_bool callingPendingFunctors_;  // 是否正在执行回调
    std::mutex mutex_;  // 保护pendingFunctors_的锁

    // ==== 内部方法 ====
    void handleRead();  // 处理wakeupFd_的可读事件
    void doPendingFunctors();  // 执行回调队列
};
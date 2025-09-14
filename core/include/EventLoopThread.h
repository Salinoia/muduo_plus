#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "NonCopyable.h"
#include "Thread.h"

class EventLoop;

class EventLoopThread : NonCopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    // ==== 核心组件 ====
    EventLoop* loop_;  // 事件循环对象指针
    Thread thread_;  // 线程对象

    // ==== 线程控制 ====
    bool exiting_;  // 退出标志
    std::mutex mutex_;  // 保护loop_的互斥锁
    std::condition_variable cond_;  // 用于loop_初始化的条件变量

    // ==== 回调函数 ====
    ThreadInitCallback callback_;  // 线程初始化回调

    // ==== 内部方法 ====
    void threadFunc();  // 线程主函数
};
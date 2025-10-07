#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "NonCopyable.h"
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : NonCopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();  // 获取所有的EventLoop

    bool isStarted() const { return started_; }  // 是否已经启动
    const std::string getName() const { return name_; }  // 获取名字

private:
    // ==== 核心组件 ====
    EventLoop* baseLoop_;  // 主事件循环（必须首位）
    std::string name_;  // 线程池名称

    // ==== 线程池状态 ====
    bool started_;  // 启动状态
    int numThreads_;  // 线程数量
    int next_;  // 轮询索引

    // ==== 线程资源 ====
    std::vector<std::unique_ptr<EventLoopThread>> threads_;  // 线程列表
    std::vector<EventLoop*> loops_;  // 子事件循环列表

    // 注：不再需要单独的ThreadInitCallback成员，因其仅在start()参数中使用
};
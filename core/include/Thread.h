#pragma once

#include <unistd.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "NonCopyable.h"

class Thread : NonCopyable {
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string& name = std::string());
    ~Thread();

    void start();
    void join();
    bool isStarted() { return started_; }
    pid_t getTid() const { return tid_; }
    const std::string& getName() const { return name_; }

    static int getNumCreated() { return numCreated_; }

private:
    // ==== 核心线程控制 ====
    std::shared_ptr<std::thread> thread_;  // 线程对象（核心资源首位）
    bool started_;  // 启动状态
    bool joined_;  // 回收状态

    // ==== 线程标识信息 ====
    pid_t tid_;  // 系统线程ID
    std::string name_;  // 线程名称

    // ==== 执行逻辑 ====
    ThreadFunc func_;  // 线程执行函数

    // ==== 类共享状态 ====
    static std::atomic_int numCreated_;  // 线程计数

    // ==== 内部方法 ====
    void setDefaultName();  // 设置默认线程名
};
#pragma once

#include <thread>
#include <memory>
#include <functional>
// #include "BlockingQueue.h"

// 前向声明，只能用作指针或者引用
template <typename T>
class BlockingQueuePro;

class ThreadPool {
public:
    // 避免隐式转换
    explicit ThreadPool(size_t thread_nums);
    ~ThreadPool();
    // Posting tasks from producer threads to the thread pool (consumer threads).
    void Post(std::function<void()> task);
private:
    void Worker();
    std::unique_ptr<BlockingQueuePro<std::function<void()>>> task_queue_; // 防止进行拷贝构造
    std::vector<std::thread> workers_;
};
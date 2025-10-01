#include "ThreadPool.h"

#include "BlockingQueue.hpp"

ThreadPool::ThreadPool(size_t thread_nums) {
    task_queue_ = std::make_unique<BlockingQueuePro<std::function<void()>>>();
    for (size_t i = 0; i < thread_nums; ++i) {
        workers_.emplace_back([this]() -> void { Worker(); });
    }
}
ThreadPool::~ThreadPool() {
    task_queue_->Cancel();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::Post(std::function<void()> task) {
    task_queue_->Push(task);
}

void ThreadPool::Worker() {
    while (true) {
        std::function<void()> task;
        if (!task_queue_->Pop(task)) {
            break;
        }
        task();
    }
}

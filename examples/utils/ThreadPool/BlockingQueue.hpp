#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

template <typename T>
class BlockingQueue {
public:
    BlockingQueue(bool nonblock = false) : nonblock_(nonblock) {}
    void Push(const T& value) {
        std::lock_guard<std::mutex> lock(prod_mtx_);
        prod_queue_.push(value);
        not_empty_.notify_one();
    }

    bool Pop(T& value) {
        std::unique_lock<std::mutex> lock(cons_mtx_);
        if (cons_queue_.empty() && SwapQueue() == 0) {
            return false;
        }
        value = cons_queue_.front();
        cons_queue_.pop();
        return true;
    }

    void Cancel() {
        std::lock_guard<std::mutex> lock(prod_mtx_);
        nonblock_ = true;
        not_empty_.notify_all();
    }

private:
    int SwapQueue() {
        std::unique_lock<std::mutex> lock(prod_mtx_);
        not_empty_.wait(lock, [this]() { return !prod_queue_.empty() || nonblock_; });
        if (prod_queue_.empty())
            return 0;
        std::swap(prod_queue_, cons_queue_);
        return cons_queue_.size();
    }
    bool nonblock_;
    std::queue<T> prod_queue_;
    std::queue<T> cons_queue_;
    std::mutex prod_mtx_;
    std::mutex cons_mtx_;
    std::condition_variable not_empty_;
};

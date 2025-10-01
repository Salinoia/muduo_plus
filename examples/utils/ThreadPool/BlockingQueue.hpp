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
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        not_empty_.notify_one();  // 唤醒一个休眠的线程
    }

    bool Pop(T& value) {  // 返回值确认是否正常弹出，引用返回元素
        std::unique_lock<std::mutex> lock(mutex_);
        /***
         * For .wait(lock, pred)
         * - 首先检查 pred：
         *   - 若 pred == true，则直接返回，lock 仍持有 mutex_（未解锁）。
         *   - 若 pred == false（如 queue_.empty() && !nonblock_），
         *     则进入阻塞逻辑：
         *       1) 在阻塞前自动 unlock mutex_
         *       2) 被 notify 唤醒后重新 lock mutex_
         *       3) 重复检查 pred，直到 pred == true
         * - 返回时，mutex_ 始终处于加锁状态。
         */
        not_empty_.wait(lock, [this]() { return !queue_.empty() || nonblock_; });
        if (queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.pop();
        return true;
    }
    void Cancel() {  // 解除阻塞在列队中的线程
        std::lock_guard<std::mutex> lock(mutex_);
        nonblock_ = true;
        not_empty_.notify_all();
    }

private:
    bool nonblock_;
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
};

template <typename T>
class BlockingQueuePro {
public:
    BlockingQueuePro(bool nonblock = false) : nonblock_(nonblock) {}
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

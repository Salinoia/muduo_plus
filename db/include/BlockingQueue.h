#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template <typename T>
class BlockingQueue {
public:
    BlockingQueue() = default;
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    // 支持右值和左值入队
    void Push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stopped_)
                return;
            queue_.push(std::move(value));
        }
        not_empty_.notify_one();
    }

    // 阻塞出队；返回 false 表示队列已停止
    bool Pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        not_empty_.wait(lock, [this]() { return !queue_.empty() || stopped_; });
        if (stopped_ && queue_.empty())
            return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 停止队列，唤醒所有等待线程
    void Cancel() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopped_ = true;
        }
        not_empty_.notify_all();
    }

    bool Empty() const {
        std::scoped_lock lock(mtx_);
        return queue_.empty();
    }

    size_t Size() const {
        std::scoped_lock lock(mtx_);
        return queue_.size();
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable not_empty_;
    std::queue<T> queue_;
    bool stopped_ = false;
};

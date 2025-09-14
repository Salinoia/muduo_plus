#pragma once
#include <atomic>

template <typename T, std::size_t Capacity>
class LockFreeQueue {
public:
    static_assert(Capacity && !(Capacity & (Capacity - 1)), "Capacity must be a power of two");
    LockFreeQueue() : read_(0), write_(0) {}
    ~LockFreeQueue() {
        std::size_t r = read_.load();
        std::size_t w = write_.load();
        while (r != w) {
            reinterpret_cast<T*>(&buffer_[r])->~T();
            r = (r + 1) & (Capacity - 1);
        }
    }

    template <typename U>
    bool Push(U&& value) {
        const std::size_t w = write_.load(std::memory_order_relaxed);
        const std::size_t next_w = (w + 1) & (Capacity - 1);
        // 检查缓冲区是否已满
        if (next_w == read_.load(std::memory_order_acquire)) {
            return false;
        }
        new (&buffer_[w]) T(std::forward<U>(value));
        write_.store(next_w, std::memory_order_release);
        return true;
    }

    bool Pop(T& value) {
        const std::size_t r = read_.load(std::memory_order_relaxed);
        // 检查缓冲区是否为空
        if (r == write_.load(std::memory_order_acquire)) {
            return false;
        }
        value = std::move(*std::launder(reinterpret_cast<T*>(&buffer_[r])));
        std::launder(reinterpret_cast<T*>(&buffer_[r]))->~T();
        read_.store((r + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    std::size_t Size() const {
        const std::size_t r = read_.load(std::memory_order_acquire);
        const std::size_t w = write_.load(std::memory_order_acquire);
        return (w >= r) ? (w - r) : (Capacity - r + w);
    }

private:
    alignas(64) std::atomic<std::size_t> read_;
    alignas(64) std::atomic<std::size_t> write_;
    alignas(64) std::aligned_storage_t<sizeof(T), alignof(T)> buffer_[Capacity];
};
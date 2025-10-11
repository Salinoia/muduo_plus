#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class MPSCLockQueue {
public:
    void Push(const T& value) { _push_impl_(value); }
    void Push(T&& value) { _push_impl_(std::move(value)); }
    bool Pop(T& value) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            not_empty_cv_.wait(lock, [this]() { 
                return !prod_queue_.empty() || !cons_queue_.empty() || stopped_.load(std::memory_order_acquire); 
            });
            if (prod_queue_.empty() && cons_queue_.empty()) {
                return false;
            }
            if (cons_queue_.empty()) {
                std::swap(prod_queue_, cons_queue_);
            }
        }
        value = std::move(cons_queue_.front());
        cons_queue_.pop();
        return true;
    }
    bool Stop() {
        stopped_.store(true, std::memory_order_release);
        not_empty_cv_.notify_all();
        return true;
    }

private:
    template <typename... Args>
    void _push_impl_(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        prod_queue_.emplace(std::forward<Args>(args)...);
        not_empty_cv_.notify_one();
    }
    std::mutex mutex_;
    std::queue<T> prod_queue_;
    std::queue<T> cons_queue_;
    std::condition_variable not_empty_cv_;
    std::atomic<bool> stopped_{false};
};

// -------- MPSC 无锁链表队列 --------
template <typename T>
class MPSCAtomicQueue {
    struct Node {
        T v;
        std::atomic<Node*> next{nullptr};
        explicit Node(T&& x) : v(std::move(x)) {}
        Node() : v() {}
    };
    std::atomic<Node*> tail_;
    Node* head_;
public:
    MPSCAtomicQueue() {
        Node* stub = new Node();
        head_ = stub;
        tail_.store(stub, std::memory_order_relaxed);
    }
    ~MPSCAtomicQueue() {
        T tmp;
        while (dequeue(tmp)) {}
        delete head_;
    }
    void enqueue(T&& x) {
        Node* n = new Node(std::move(x));
        n->next.store(nullptr, std::memory_order_relaxed);
        Node* p = tail_.exchange(n, std::memory_order_acq_rel);
        p->next.store(n, std::memory_order_release);
    }
    bool dequeue(T& out) {
        Node* next = head_->next.load(std::memory_order_acquire);
        if (!next) return false;
        out = std::move(next->v);
        delete head_;
        head_ = next;
        return true;
    }
    template<typename F>
    size_t drain(F&& fn, size_t max_items = SIZE_MAX) {
        size_t n = 0; T item;
        while (n < max_items && dequeue(item)) { fn(std::move(item)); ++n; }
        return n;
    }
};
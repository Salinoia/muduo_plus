#include "Thread.h"

#include <future>

#include "CurrentThread.h"

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name) : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name) {
    setDefaultName();
}

Thread::~Thread() {
    if (started_ && !joined_ && thread_ && thread_->joinable()) {
        thread_->detach();  // 分离线程的方法进行销毁，非阻塞式
        // thread_->join(); // 有资源或者行为风险，join更加稳定
    }
}

void Thread::start() {
    if (started_) {
        return;
    }
    std::promise<pid_t> tidPromise;
    auto tidFuture = tidPromise.get_future();
    thread_ = std::make_shared<std::thread>([this, promise = std::move(tidPromise)] () mutable {
        tid_ = CurrentThread::tid();
        promise.set_value(tid_);
        func_();
    });
    tid_ = tidFuture.get();
    started_ = true;
}

void Thread::join() {
    if (joined_ || !thread_) {
        return;
    }
    if (thread_->joinable()) {
        thread_->join();
        joined_ = true;
    }
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if (name_.empty()) {
        name_ = "Thread" + std::to_string(++numCreated_);
    }
}

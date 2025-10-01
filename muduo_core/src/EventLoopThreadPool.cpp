#include "EventLoopThreadPool.h"

#include "EventLoopThread.h"
#include "Logger.h"
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg) : baseLoop_(baseLoop), name_(nameArg), started_(false), numThreads_(0), next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    // 不删除循环，因为这里是栈区变量
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    started_ = true;
    if (numThreads_ == 0 && cb) {  // 单线程
        cb(baseLoop_);
    }
    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i + 1);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());
    }
}
// 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
EventLoop* EventLoopThreadPool::getNextLoop() {
    // 单线程， mainReactor 存在，subReactor 不存在
    EventLoop* loop = baseLoop_;
    if (!loops_.empty()) {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

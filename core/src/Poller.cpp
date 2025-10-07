#include "Poller.h"

#include "Channel.h"
#include "EPollPoller.h"

Poller::Poller(EventLoop* loop) : ownerLoop_(loop) {}

bool Poller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->getFd());
    return it != channels_.end() && it->second == channel;
}

// 静态方法
Poller* Poller::newDefaultPoller(EventLoop* loop) { 
    // 通过环境变量决定使用 poll 还是 epoll
    if (::getenv("MUDUO_USE_POLL")) {
        return nullptr;  // 此处未实现对于poll接口的支持
    } else {
        return new EPollPoller(loop);
    }
}
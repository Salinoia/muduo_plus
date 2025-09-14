#pragma once

#include <sys/epoll.h>

#include <vector>

#include "Poller.h"
#include "TimeStamp.h"

// epoll使用三个功能：
// 1.epoll_create
// 2.epoll_ctl(add, mod, del)
// 3.epoll_wait
class Channel;
class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    TimeStamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    using EventList = std::vector<epoll_event>;
    // ==== 核心epoll资源 ====
    int epollfd_;  // epoll实例的文件描述符（核心资源首位）
    EventList events_;  // epoll_wait返回的事件缓冲区

    // ==== 常量配置 ====
    static const int kInitEventListSize = 16;  // 事件列表初始大小

    // ==== 内部方法 ====
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);
};
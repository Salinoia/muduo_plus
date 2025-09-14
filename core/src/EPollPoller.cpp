#include "EPollPoller.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "Channel.h"
#include "Logger.h"

enum {
    kNew = -1,  // Channel成员index_初始化为-1，此时表示还未添加至Poller
    kAdded = 1,  // 已添加
    kDeleted = 2  // 已删除
};

EPollPoller::EPollPoller(EventLoop* loop) : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

TimeStamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    // 由于频繁调用poll 当遇到并发场景 关闭DEBUG日志提升效率
    // LOG_INFO("func = %s => fd total count: %lu\n", __FUNCTION__, channels_.size());
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    TimeStamp now(TimeStamp::now());
    if (numEvents > 0) {
        LOG_INFO("%d events happend\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("%s timeout \n", __FUNCTION__);
    } else {
        if (saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel

// ```mermaid
//  graph LR
//  A[Channel.update/remove] --> B[EventLoop.updateChannel/removeChannel]
//  B --> C[EPollPoller.updateChannel/removeChannel]
// ```
void EPollPoller::updateChannel(Channel* channel) {
    const int index = channel->getIndex();
    LOG_INFO("func = %s => fd = %d events = %d index = %d\n", __FUNCTION__, channel->getFd(), channel->getEvents(), index);

    if (index == kNew || index == kDeleted) {  // channel还未在Poller中注册
        if (index == kNew) {
            int fd = channel->getFd();
            channels_[fd] = channel;
        } else {  // index == kDeleted
            // 仅更新状态，无需操作channels_
        }
        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {  // channel已经在Poller中注册过
        int fd = channel->getFd();
        if (channel->isNoneEvent()) {  // 为空事件，删除该fd
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel) {
    int fd = channel->getFd();
    channels_.erase(fd);

    LOG_INFO("func = %s => fd = %d", __FUNCTION__, fd);
    int index = channel->getIndex();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(kDeleted);
}

// 将 epoll_wait 返回的就绪事件填充到 activeChannels 中，供 EventLoop 处理。
// 每个 Channel 关联的文件描述符上发生了事件（events_[i].events），
// 并已通过 setRevents() 更新到 Channel 的状态中。
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);  // 记录实际发生的事件
        activeChannels->push_back(channel);  // 添加到就绪列表
    }
}

void EPollPoller::update(int operation, Channel* channel) {
    epoll_event event;
    ::memset(&event, 0, sizeof(event));
    int fd = channel->getFd();

    event.events = channel->getEvents();
    event.data.fd = fd;
    event.data.ptr = channel;
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d \n", errno);
        } else {
            LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);
        }
    }
}

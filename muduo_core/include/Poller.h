#pragma once

#include <unordered_map>
#include <vector>

#include "NonCopyable.h"
#include "TimeStamp.h"

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
// 此处定义为类似于接口类，用于实现后续的epoll方法
class Poller {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口，定义为纯虚函数强制要求派生类实现功能
    virtual TimeStamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断参数channel是否在当前的Poller当中
    bool hasChannel(Channel* channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    // map的kv值: key:sockfd value:sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;  // 定义Poller所属的事件循环EventLoop
};
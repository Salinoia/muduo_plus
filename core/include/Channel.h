#pragma once
#include <functional>
#include <memory>

#include "NonCopyable.h"
#include "TimeStamp.h"

class InetAddress;
class EventLoop;

// Channel类：封装文件描述符和事件回调，负责注册/删除监听事件并处理IO事件
class Channel : NonCopyable {
public:
    using EventCallback = std::function<void()>;  // 普通事件回调类型
    using ReadEventCallback = std::function<void(TimeStamp)>;  // 读事件回调(带时间戳)

    Channel(EventLoop* loop, int fd);  // 构造函数(所属EventLoop和文件描述符)
    ~Channel();  // 析构时自动移除监听

    // 处理事件(EventLoop发现事件后调用)
    void handleEvent(TimeStamp receiveTime);

    // 设置各类事件回调函数
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }  // 设置写回调
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }  // 设置关闭回调
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }  // 设置错误回调
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }  // 设置读回调

    // 绑定共享指针(防止Channel被删除时还在执行回调)
    void tie(const std::shared_ptr<void>&);

    // 获取基本信息
    int getFd() const { return fd_; }  // 获取文件描述符
    int getEvents() const { return events_; }  // 获取监听的事件集合
    void setRevents(int revt) { revents_ = revt; }  // 设置实际发生的事件(Poller设置)

    // 事件监听控制(内部会调用update)
    void enableReading() {  // 启用读监听
        if (fd_ < 0)
            return;
        events_ |= kReadEvent;
        update();
    }
    void disableReading() {  // 禁用读监听
        if (fd_ < 0)
            return;
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting() {  // 启用写监听
        if (fd_ < 0)
            return;
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting() {  // 禁用写监听
        if (fd_ < 0)
            return;
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll() {  // 禁用所有监听
        if (fd_ < 0)
            return;
        events_ = kNoneEvent;
        update();
    }

    // 事件状态查询
    bool isNoneEvent() const { return events_ == kNoneEvent; }  // 是否无监听事件
    bool isWriting() const { return events_ & kWriteEvent; }  // 是否监听写事件
    bool isReading() const { return events_ & kReadEvent; }  // 是否监听读事件

    // 用于Poller的索引管理
    int getIndex() { return index_; }  // 获取在Poller中的索引
    void setIndex(int idx) { index_ = idx; }  // 设置Poller中的索引

    EventLoop* ownerLoop() { return loop_; }  // 获取所属EventLoop
    void remove();  // 从EventLoop移除当前Channel
private:
    // ==== 核心描述符与事件状态 ====
    EventLoop* loop_;  // 所属事件循环（必须首位，因其他成员依赖它）
    const int fd_;  // 管理的文件描述符
    int events_;  // 注册监听的事件（EPOLLIN/EPOLLOUT等）
    int revents_;  // Poller返回的实际发生事件
    int index_;  // 在Poller中的状态索引（如EPOLL_CTL_ADD/MOD）

    // ==== 资源安全控制 ====
    std::weak_ptr<void> tie_;  // 弱引用绑定，防止回调时Channel被销毁
    bool tied_;  // 是否已绑定

    // ==== 事件回调函数 ====
    ReadEventCallback readCallback_;  // 读回调（高频访问，置前）
    EventCallback writeCallback_;  // 写回调
    EventCallback closeCallback_;  // 关闭回调
    EventCallback errorCallback_;  // 错误回调

    // ==== 静态常量事件标志 ====
    static const int kNoneEvent;  // 无事件标志
    static const int kReadEvent;  // 读事件标志
    static const int kWriteEvent;  // 写事件标志

    // ==== 内部方法 ====
    void update();  // 更新事件监听状态
    void handleEventWithGuard(TimeStamp receiveTime);  // 执行回调（带保护）
};

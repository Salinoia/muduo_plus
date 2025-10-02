# My Muduo 网络库项目

## 项目概述

本项目是基于陈硕的 muduo 网络库实现的一个简化版 Reactor 模式网络库，采用 C++14 编写。实现了高性能的多线程 TCP 网络服务器核心框架，支持多 Reactor 模型和事件驱动编程。

## 项目结构

### 主要目录结构

```
.
├── include/                # 头文件目录
│   ├── Acceptor.h          # 接受新连接组件
│   ├── Buffer.h            # 应用层缓冲区
│   ├── Callbacks.h         # 回调函数类型定义
│   ├── Channel.h           # 事件分发器
│   ├── CurrentThread.h     # 线程相关工具
│   ├── EPollPoller.h       # epoll 实现
│   ├── EventLoop.h         # 事件循环核心类
│   ├── EventLoopThread.h   # 事件循环线程
│   ├── EventLoopThreadPool.h # 线程池管理
│   ├── InetAddress.h       # 网络地址封装
│   ├── Logger.h            # 日志工具
│   ├── NonCopyable.h       # 不可拷贝基类
│   ├── Poller.h            # Poller 抽象接口
│   ├── Socket.h            # Socket 封装
│   ├── TcpConnection.h     # TCP 连接
│   ├── TcpServer.h         # TCP 服务器
│   ├── Thread.h            # 线程封装
│   └── Timestamp.h         # 时间戳工具
├── src/                    # 源文件目录
│   └── (对应头文件的实现)
├── example/                # 示例代码
│   ├── EchoServer.cpp      # 回显服务器示例
│   └── CMakeLists.txt
└── CMakeLists.txt          # 项目构建文件
```

## 核心特性

1. **多 Reactor 模型**：
   - 主 Reactor 负责接受新连接
   - 子 Reactor 负责处理已建立连接的 I/O 事件
   - 支持配置多个 I/O 线程

2. **事件驱动架构**：
   - 基于 epoll 的 I/O 多路复用
   - 非阻塞 I/O
   - 定时器支持

3. **线程模型**：
   - one loop per thread 设计
   - 线程安全的跨线程调用
   - 无锁设计减少竞争

4. **高性能设计**：
   - 应用层缓冲区减少系统调用
   - 智能指针管理对象生命周期
   - 避免数据拷贝

## 构建说明

### 依赖项

- Linux 系统
- CMake (>= 3.10)
- g++ (支持 C++14)

### 构建步骤

```bash
mkdir build && cd build
cmake ..
make
```

### 运行示例

构建完成后，可以在 `example/` 目录下找到示例程序：

```bash
./example/echo
```

## 示例代码

一个简单的 Echo 服务器实现（位于 `example/EchoServer.cpp`）：

```cpp
#include "Logger.h"
#include "TcpServer.h"

class EchoServer : public std::enable_shared_from_this<EchoServer> {
public:
    // ... 省略部分代码 ...
    
    void start() {
        auto self = shared_from_this();
        server_.setConnectionCallback([self](const TcpConnectionPtr& conn) {
            self->onConnection(conn);
        });
        
        server_.setMessageCallback([self](const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
            self->onMessage(conn, buf, time);
        });
        
        server_.setThreadNum(4);
        server_.start();
    }
    
    // ... 省略部分代码 ...
};

int main() {
    EventLoop loop;
    InetAddress addr("127.0.0.1", 8000);
    auto server = EchoServer::create(&loop, addr, "EchoServer");
    server->start();
    loop.loop();
    return 0;
}
```

## 设计理念

1. **基于事件的非阻塞网络编程**
2. **线程安全的回调机制**
3. **资源管理的 RAII 原则**
4. **避免数据拷贝的高效设计**
5. **清晰的接口分层**

## 未来计划

- [ ] 增加 HTTP 协议支持
- [ ] 实现 WebSocket 支持
- [ ] 添加更多的性能测试
- [ ] 完善文档和示例
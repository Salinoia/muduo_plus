#pragma once

#include <functional>
#include <memory>

// 前向声明（Forward Declarations）
class Buffer;  // 网络数据缓冲区类
class TcpConnection;  // TCP连接类
class TimeStamp;  // 时间戳工具类

// 智能指针类型别名
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * 回调函数类型定义（Callback Type Definitions）
 * 所有回调均采用const TcpConnectionPtr&形式，避免对象拷贝
 */

// 连接建立/关闭回调
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;  // 连接建立时触发
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;  // 连接关闭时触发

// 数据发送回调
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;  //  数据完全写入内核缓冲区时触发

// 流量控制回调
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;  // 发送缓冲区超过高水位阈值时触发

// 数据到达回调（带缓冲区和时间戳）
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, TimeStamp)>;  // 接收到新数据时触发（带接收缓冲区和时间戳）

#pragma once
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "NonCopyable.h"
#include "SpscRingBuffer.h"

// 定义日志级别
enum LogLevel {
    TRACE,  // 新增：流程追踪
    DEBUG,  // 调试信息
    INFO,  // 普通信息
    WARN,  // 警告信息
    ERROR,  // 错误信息
    FATAL,  // core dump信息
};
// 线程局部存储（TLS）缓存 Logger 实例
class Logger : NonCopyable {
public:
    static Logger& instance();  // 获取全局单例（带线程本地缓存）

    void setLogLevel(LogLevel level);  // 设置日志级别
    void log(LogLevel level, const std::string& msg);  // 记录日志
    LogLevel getLogLevel() const { return logLevel_; }

    void setOutputToConsole(bool enable);
    void setOutputToFile(const std::string& filename);

    // enable asynchronous logging; call once before logging
    void enableAsync(bool enable = true);
    // set log rolling size in bytes
    void setRollSize(size_t bytes) { rollSize_ = bytes; }

    // Attach a trace identifier to subsequent log lines on the current thread.
    void setTraceId(const std::string& id);
    void clearTraceId();

private:
    Logger();  // 私有构造函数（单例模式）
    ~Logger();

    LogLevel logLevel_ = INFO;  // 默认日志级别
    std::mutex mutex_;  // 保护日志输出

    // 输出目标
    bool consoleOutput_ = true;
    std::unique_ptr<std::ofstream> fileOutput_;

    // asynchronous logging
    bool async_ = false;
    std::atomic<bool> running_{false};
    std::thread worker_;
    LockFreeQueue<std::string, 4096> queue_;

    // file rolling
    size_t rollSize_ = 10 * 1024 * 1024;  // 10 MB default
    size_t fileSize_ = 0;
    int fileIndex_ = 0;
    std::string baseFilename_;
    std::tm currentDate_{};

    void asyncWriteLoop();
    void rollFileIfNeeded(const std::tm& tm);
    void openLogFile(const std::tm& tm);

    static thread_local std::string traceId_;
};

// 日志宏（自动附加日志级别、线程安全）
#define LOG_TRACE(fmt, ...)                        \
    if (Logger::instance().getLogLevel() <= TRACE) \
    Logger::instance().log(TRACE, formatLog(fmt, ##__VA_ARGS__))
#ifdef MUDEBUG
#    define LOG_DEBUG(fmt, ...) Logger::instance().log(DEBUG, formatLog(fmt, ##__VA_ARGS__))
#else
#    define LOG_DEBUG(fmt, ...)  // 空宏（生产环境不生效）
#endif
#define LOG_INFO(fmt, ...) Logger::instance().log(INFO, formatLog(fmt, ##__VA_ARGS__))
#define LOG_WARN(fmt, ...) Logger::instance().log(WARN, formatLog(fmt, ##__VA_ARGS__))
#define LOG_ERROR(fmt, ...) Logger::instance().log(ERROR, formatLog(fmt, ##__VA_ARGS__))
#define LOG_FATAL(fmt, ...) Logger::instance().log(FATAL, formatLog(fmt, ##__VA_ARGS__))

// 格式化字符串（防止缓冲区溢出）
std::string formatLog(const char* fmt, ...);
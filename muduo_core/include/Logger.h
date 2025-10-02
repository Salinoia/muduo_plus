#pragma once

#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>

#include "NonCopyable.h"

// 日志级别
enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
};

class Logger : NonCopyable {
public:
    // 单例获取
    static Logger& instance();

    // 设置日志级别
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    // 输出目标设置
    void setOutputToConsole(bool enable);
    void setOutputToFile(const std::string& filename);

    // 核心接口
    void log(LogLevel level, const std::string& msg);

    // 模板化便捷接口（支持 std::format + source_location）
    template <typename... Args>
    void logWithLocation(LogLevel level, std::string_view fmt, Args&&... args) {
        logWithLocImpl(level, std::source_location::current(), fmt, std::forward<Args>(args)...);
    }

    // 便捷调用
    template <typename... Args>
    void trace(std::string_view fmt, Args&&... args) {
        logWithLocation(LogLevel::TRACE, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void debug(std::string_view fmt, Args&&... args) {
        logWithLocation(LogLevel::DEBUG, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void info(std::string_view fmt, Args&&... args) {
        logWithLocation(LogLevel::INFO, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void warn(std::string_view fmt, Args&&... args) {
        logWithLocation(LogLevel::WARN, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void error(std::string_view fmt, Args&&... args) {
        logWithLocation(LogLevel::ERROR, fmt, std::forward<Args>(args)...);
    }
    template <typename... Args>
    void fatal(std::string_view fmt, Args&&... args) {
        logWithLocation(LogLevel::FATAL, fmt, std::forward<Args>(args)...);
    }

private:
    Logger();
    ~Logger();

    static const char* levelToString(LogLevel level);

    LogLevel logLevel_ = LogLevel::INFO;
    mutable std::mutex mutex_;
    bool consoleOutput_ = true;
    std::unique_ptr<std::ofstream> fileOutput_;

private:
    template <typename... Args>
    void logWithLocImpl(LogLevel level, const std::source_location& loc, std::string_view fmt, Args&&... args) {
        auto formatted = std::vformat(fmt, std::make_format_args(args...));
        log(level, std::format("[{}:{}:{}] {}", loc.file_name(), loc.function_name(), loc.line(), formatted));
    }
};

#pragma once

#include <atomic>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>

#include "NonCopyable.h"

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

class AsyncFileSink;
class Logger : NonCopyable {
private:
    void log(LogLevel level, std::string_view msg);

    // loc 由宏在调用点注入；这里仅做拼装与早退
    template <typename... Args>
    void logWithLocImpl(LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        if (level < logLevel_.load(std::memory_order_relaxed))
            return;  // 低级别早退

        std::string user = std::format(fmt, std::forward<Args>(args)...);
        std::string head = formatLocCompact(loc);
        log(level, std::format("{} {}", head, user));
    }

public:
    static Logger& instance();

    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;

    void setOutputToConsole(bool enable);
    void setOutputToFile(const std::string& filename);
    void setOutputToFileAsync(const std::string& filename);

    // 供宏调用：必须显式传 loc 才能拿到真实调用点
    template <typename... Args>
    void logWithLocation(LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt, Args&&... args) {
        logWithLocImpl(level, loc, fmt, std::forward<Args>(args)...);
    }

private:
    Logger();
    ~Logger();

    static const char* levelToString(LogLevel level);

    // 将 [绝对路径/参数展开] → [短文件名:短函数名:行]
    static std::string formatLocCompact(const std::source_location& loc);

    std::atomic<LogLevel> logLevel_{LogLevel::INFO};
    mutable std::mutex mutex_;
    bool consoleOutput_ = true;
    std::unique_ptr<std::ofstream> fileOutput_;
    std::unique_ptr<AsyncFileSink> asyncSink_;
};

#include "Logger.h"

#include <libgen.h>

#include <cstdarg>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
namespace {
thread_local Logger* tlsLogger = nullptr;  // 每个线程缓存自己的实例

const char* levelToString(LogLevel level) {
    static const char* strings[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    return strings[level];
}
}  // namespace
Logger& Logger::instance() {
    if (tlsLogger == nullptr) {
        static Logger gloLogger;  // 全局唯一单例
        tlsLogger = &gloLogger;  // 线程首次访问的时候绑定
    }
    return *tlsLogger;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < logLevel_)
        return;  // 低于当前日志级别直接忽略
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = {};
    localtime_r(&time, &tm);  // Linux/Unix下线程安全版本
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [0x" << std::hex << std::this_thread::get_id() << "] " << "[";
    switch (level) {
    case TRACE:
        oss << "TRACE";
        break;
    case DEBUG:
        oss << "DEBUG";
        break;
    case WARN:
        oss << "WARN";
        break;
    case INFO:
        oss << "INFO";
        break;
    case ERROR:
        oss << "ERROR";
        break;
    case FATAL:
        oss << "FATAL";
        break;
    }
    oss << "] " << msg << std::endl;

    // 输出日志
    if (consoleOutput_) {
        std::cout << oss.str();
    }
    if (fileOutput_) {
        *fileOutput_ << oss.str();
        fileOutput_->flush();  // 立即刷新到文件
    }

    // FATAL 日志终止程序
    if (level == FATAL) {
        std::abort();
    }
}

void Logger::setOutputToConsole(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    consoleOutput_ = enable;
}

void Logger::setOutputToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    fileOutput_ = std::make_unique<std::ofstream>(filename, std::ios::app);  // make_unique 是 C++14 的特性
    // fileOutput_.reset(new std::ofstream(filename, std::ios::app));
    if (!fileOutput_->good()) {
        fileOutput_.reset();
        throw std::runtime_error("Failed to open log file");
    }
}

Logger::Logger() {
    consoleOutput_ = true;
}

Logger::~Logger() {
    if (fileOutput_) {
        fileOutput_->close();
    }
}
// 安全的格式化函数（替代 snprintf）
std::string formatLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

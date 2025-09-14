#include "Logger.h"

#include <libgen.h>

#include <cstdarg>
#include <ctime>
#include <filesystem>
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

thread_local std::string Logger::traceId_{};
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
    std::string finalMsg = msg;
    if (!traceId_.empty()) {
        finalMsg = "[TraceId:" + traceId_ + "] " + msg;
    }
    oss << "] " << finalMsg << std::endl;
    std::string line = oss.str();

    if (async_) {
        while (!queue_.Push(line)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        rollFileIfNeeded(tm);
        if (consoleOutput_) {
            std::cout << line;
        }
        if (fileOutput_) {
            *fileOutput_ << line;
            fileSize_ += line.size();
            fileOutput_->flush();
        }
    }

    if (level == FATAL) {
        std::abort();
    }
}

void Logger::setTraceId(const std::string& id) { traceId_ = id; }

void Logger::clearTraceId() { traceId_.clear(); }

void Logger::setOutputToConsole(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    consoleOutput_ = enable;
}

void Logger::setOutputToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    baseFilename_ = filename;
    fileIndex_ = 0;
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = {};
    localtime_r(&time, &tm);
    openLogFile(tm);
}

Logger::Logger() {
    consoleOutput_ = true;
}

Logger::~Logger() {
    if (async_) {
        running_.store(false);
        if (worker_.joinable()) {
            worker_.join();
        }
    }
    if (fileOutput_) {
        fileOutput_->close();
    }
}

void Logger::enableAsync(bool enable) {
    if (enable && !async_) {
        async_ = true;
        running_.store(true);
        worker_ = std::thread([this]() { asyncWriteLoop(); });
    }
}

void Logger::asyncWriteLoop() {
    std::string line;
    while (running_.load() || queue_.Size() > 0) {
        if (queue_.Pop(line)) {
            auto now = std::chrono::system_clock::now();
            std::time_t time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = {};
            localtime_r(&time, &tm);
            std::lock_guard<std::mutex> lock(mutex_);
            rollFileIfNeeded(tm);
            if (consoleOutput_) {
                std::cout << line;
            }
            if (fileOutput_) {
                *fileOutput_ << line;
                fileSize_ += line.size();
                fileOutput_->flush();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void Logger::openLogFile(const std::tm& tm) {
    if (baseFilename_.empty()) return;

    std::filesystem::path path(baseFilename_);
    std::filesystem::create_directories(path.parent_path());

    std::string stem = path.stem().string();
    std::string ext = path.extension().string();
    std::ostringstream oss;
    oss << stem << '_' << std::put_time(&tm, "%Y%m%d") << '_' << std::setw(3) << std::setfill('0') << fileIndex_++ << ext;
    std::filesystem::path newPath = path.parent_path() / oss.str();

    fileOutput_ = std::make_unique<std::ofstream>(newPath.string(), std::ios::app);
    if (!fileOutput_->good()) {
        fileOutput_.reset();
        throw std::runtime_error("Failed to open log file");
    }
    fileSize_ = fileOutput_->tellp();
    currentDate_ = tm;
}

void Logger::rollFileIfNeeded(const std::tm& tm) {
    if (!fileOutput_) return;
    if (tm.tm_year != currentDate_.tm_year || tm.tm_mon != currentDate_.tm_mon || tm.tm_mday != currentDate_.tm_mday || fileSize_ >= rollSize_) {
        fileOutput_->close();
        openLogFile(tm);
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

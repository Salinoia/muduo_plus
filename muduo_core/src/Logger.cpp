#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

Logger& Logger::instance() {
    static Logger globalLogger;
    return globalLogger;
}

Logger::Logger() : consoleOutput_(true) {}

Logger::~Logger() {
    if (fileOutput_)
        fileOutput_->close();
}

void Logger::setLogLevel(LogLevel level) {
    logLevel_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::getLogLevel() const {
    return logLevel_.load(std::memory_order_relaxed);
}

void Logger::setOutputToConsole(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    consoleOutput_ = enable;
}

void Logger::setOutputToFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    fileOutput_ = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (!fileOutput_ || !fileOutput_->good()) {
        fileOutput_.reset();
        consoleOutput_ = true;
        std::cerr << "Logger: failed to open file, fallback to console\n";
    }
}

static inline std::string_view basename_view(std::string_view path) {
    const size_t pos = path.find_last_of("/\\");
    return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
}

static inline std::string_view unqual_func(std::string_view fn) {
    // 去掉命名空间/类限定
    const size_t scope = fn.rfind("::");
    if (scope != std::string_view::npos)
        fn = fn.substr(scope + 2);
    // 去掉参数列表
    const size_t paren = fn.find('(');
    if (paren != std::string_view::npos)
        fn = fn.substr(0, paren);
    // 去掉模板实参展开（只保留名，不保留 <...>）
    const size_t angle = fn.find('<');
    if (angle != std::string_view::npos)
        fn = fn.substr(0, angle);
    return fn;
}

std::string Logger::formatLocCompact(const std::source_location& loc) {
    std::string_view file = basename_view(loc.file_name());
    std::string_view func = unqual_func(loc.function_name());
    return std::format("[{}:{}:{}]", file, func, loc.line());
}

void Logger::log(LogLevel level, std::string_view msg) {
    if (level < logLevel_.load(std::memory_order_relaxed))
        return;

    std::ostringstream oss;
    const auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [tid:" << std::hash<std::thread::id>()(std::this_thread::get_id()) << "] "
        << "[" << levelToString(level) << "] " << msg << "\n";

    const std::string out = oss.str();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (consoleOutput_)
            std::cout << out;
        if (fileOutput_) {
            *fileOutput_ << out;
            fileOutput_->flush();
        }
    }

    if (level == LogLevel::FATAL)
        std::abort();
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::TRACE:
        return "TRACE";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    }
    return "UNKNOWN";
}

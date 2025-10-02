#include "Logger.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

// 单例
Logger& Logger::instance() {
    static Logger globalLogger;
    return globalLogger;
}

Logger::Logger() : consoleOutput_(true) {}

Logger::~Logger() {
    if (fileOutput_) {
        fileOutput_->close();
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    logLevel_ = level;
}

LogLevel Logger::getLogLevel() const {
    return logLevel_;
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

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < logLevel_)
        return;

    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&time, &tm);

    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [tid:" << std::hash<std::thread::id>()(std::this_thread::get_id()) << "] "
        << "[" << levelToString(level) << "] " << msg << "\n";

    const std::string output = oss.str();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (consoleOutput_) {
            std::cout << output;
        }
        if (fileOutput_) {
            *fileOutput_ << output;
            fileOutput_->flush();
        }
    }

    if (level == LogLevel::FATAL) {
        std::abort();
    }
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

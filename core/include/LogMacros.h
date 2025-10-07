#pragma once
#include <source_location>

#include "Logger.h"

#define LOG_TRACE(fmt, ...) ::Logger::instance().logWithLocation(LogLevel::TRACE, std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) ::Logger::instance().logWithLocation(LogLevel::DEBUG, std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) ::Logger::instance().logWithLocation(LogLevel::INFO, std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) ::Logger::instance().logWithLocation(LogLevel::WARN, std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Logger::instance().logWithLocation(LogLevel::ERROR, std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) ::Logger::instance().logWithLocation(LogLevel::FATAL, std::source_location::current(), fmt, ##__VA_ARGS__)

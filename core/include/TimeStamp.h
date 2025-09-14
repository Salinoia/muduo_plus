#pragma once

#include <chrono>  // 引入时间处理库
#include <iomanip>  // 引入格式化输出
#include <sstream>  // 引入字符串流
#include <string>  // 引入字符串类

// TimeStamp类，用来表示时间戳（微秒级）
class TimeStamp {
private:
    int64_t microSecondsSinceEpoch_;  // 从1970年1月1日00:00:00 UTC起的微秒数

public:
    using Clock = std::chrono::system_clock;  // 使用系统时钟（标准实时时钟）
    using MicroSeconds = std::chrono::microseconds;  // 微秒单位别名

    // 默认构造函数，初始化为0
    TimeStamp() : microSecondsSinceEpoch_(0) {}

    // 显式构造函数，使用指定微秒数初始化
    explicit TimeStamp(int64_t microSecondsEpoch) : microSecondsSinceEpoch_(microSecondsEpoch) {}

    // 静态成员函数，返回当前时间的TimeStamp对象
    static TimeStamp now() {
        auto now = Clock::now();  // 获取当前系统时间点
        return TimeStamp(std::chrono::duration_cast<MicroSeconds>(now.time_since_epoch()).count());
        // 将当前时间点转换成微秒数，并构造TimeStamp
    }
    // 将TimeStamp对象转换成字符串，格式为"YYYY/MM/DD HH:MM:SS"
    std::string toString() const {
        auto timePoint = Clock::time_point{MicroSeconds(microSecondsSinceEpoch_)};
        // 把微秒数重新转为时间点

        std::time_t time = Clock::to_time_t(timePoint);  // 转为time_t类型，方便格式化
        std::tm tm = {};  // 定义一个tm结构体用于保存时间信息

        localtime_r(&time, &tm);  // Linux/Unix下线程安全版本

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");  // 格式化时间为字符串
        return oss.str();  // 返回格式化后的字符串
    }
    int64_t getMicroSecondsSinceEpoch() const{
        return microSecondsSinceEpoch_;
    }
};

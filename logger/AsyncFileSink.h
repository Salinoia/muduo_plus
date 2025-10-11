#pragma once

#include <string>
#include <thread>

#include "MPSCQueue.h"

class AsyncFileSink {
public:
    struct Options {
        size_t sync_bytes = 4 * 1024 * 1024;  // 达到字节阈值时触发刷盘
        size_t batch_iov_max = 1024;  // 单批最多写多少条日志
        int sync_interval_ms = 1000;  // 定时刷盘间隔（毫秒）
        bool use_fdatasync = true;  // true = fdatasync, false = fsync
        Options() {}
        Options(size_t sb, size_t bi, int si, bool uf) : sync_bytes(sb), batch_iov_max(bi), sync_interval_ms(si), use_fdatasync(uf) {}
    };

    // 构造函数：传入文件路径和配置
    explicit AsyncFileSink(const std::string& path, Options opt = {});
    ~AsyncFileSink();

    // 提交一条完整日志（建议已包含 '\n'）
    void submit(std::string&& line);

    // 停止后台线程并确保剩余日志写盘
    void stop();

    // 在 FATAL 等场景调用：强制立即写盘并同步落地
    void flush_all_now();

private:
    // 后台线程执行循环
    void run();

    int fd_{-1};  // 文件描述符
    Options opt_;  // 配置参数
    std::atomic<bool> running_{false};
    std::thread worker_;  // 后台线程

    std::unique_ptr<MPSCAtomicQueue<std::string>> queue_;

    std::condition_variable cv_;
    std::mutex cv_mtx_;
};

#include "AsyncFileSink.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include <chrono>
#include <deque>
#include <iostream>

#include "Buffer.h"

AsyncFileSink::AsyncFileSink(const std::string& path, Options opt) : opt_(opt), queue_(std::make_unique<MPSCAtomicQueue<std::string>>()) {
    fd_ = ::open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, 0644);
    if (fd_ < 0) {
        throw std::runtime_error("AsyncFileSink: failed to open log file");
    }
    running_.store(true, std::memory_order_relaxed);
    worker_ = std::thread(&AsyncFileSink::run, this);
}

AsyncFileSink::~AsyncFileSink() {
    stop();
}

void AsyncFileSink::submit(std::string&& line) {
    queue_->enqueue(std::move(line));
    { std::lock_guard<std::mutex> lk(cv_mtx_); }
    cv_.notify_one();
}

void AsyncFileSink::stop() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        cv_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }
}

void AsyncFileSink::flush_all_now() {
    std::deque<std::string> tmp;
    queue_->drain([&](std::string&& s) { tmp.emplace_back(std::move(s)); }, SIZE_MAX);
    for (auto& s : tmp) {
        ::write(fd_, s.data(), s.size());
    }
    if (opt_.use_fdatasync) {
        ::fdatasync(fd_);
    } else {
        ::fsync(fd_);
    }
}

void AsyncFileSink::run() {
    using clock = std::chrono::steady_clock;
    auto next_sync = clock::now() + std::chrono::milliseconds(opt_.sync_interval_ms);

    Buffer buf(64 * 1024);
    size_t bytes_since_sync = 0;

    while (running_.load(std::memory_order_relaxed)) {
        std::string line;
        if (!queue_->dequeue(line)) {
            std::unique_lock<std::mutex> lk(cv_mtx_);
            cv_.wait_until(lk, next_sync);
        } else {
            buf.append(line.data(), line.size());
            bytes_since_sync += line.size();
        }

        auto now = clock::now();
        if (buf.readableBytes() >= 64 * 1024 || bytes_since_sync >= opt_.sync_bytes || now >= next_sync) {
            int err = 0;
            ssize_t n = buf.writeFd(fd_, &err);
            (void) n;  // ignore partial write for brevity
            buf.retrieveAll();

            if (opt_.use_fdatasync)
                ::fdatasync(fd_);
            else
                ::fsync(fd_);

            bytes_since_sync = 0;
            next_sync = now + std::chrono::milliseconds(opt_.sync_interval_ms);
        }
    }

    // 最后一轮排空
    std::string line;
    while (queue_->dequeue(line)) {
        buf.append(line.data(), line.size());
    }
    if (buf.readableBytes() > 0) {
        int err = 0;
        buf.writeFd(fd_, &err);
    }
    if (opt_.use_fdatasync)
        ::fdatasync(fd_);
    else
        ::fsync(fd_);
}

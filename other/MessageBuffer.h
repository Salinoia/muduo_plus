#pragma once
#include <errno.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
class MessageBuffer {
public:
    MessageBuffer() : buffer_(4096), rpos_(0), wpos_(0) {}
    explicit MessageBuffer(std::size_t size) : buffer_(size), rpos_(0), wpos_(0) {}
    MessageBuffer(const MessageBuffer&) = delete;
    MessageBuffer& operator=(const MessageBuffer&) = delete;
    MessageBuffer(MessageBuffer&& other) noexcept : buffer_(std::move(other.buffer_)), rpos_(std::exchange(other.rpos_, 0)), wpos_(std::exchange(other.wpos_, 0)) {}
    MessageBuffer& operator=(MessageBuffer&& other) noexcept {
        if (this != &other) {  // a = std::move(a); 本质上是未指定行为，这里加上保护是纯粹的防御性编程
            buffer_ = std::move(other.buffer_);
            rpos_ = std::exchange(other.rpos_, 0);
            wpos_ = std::exchange(other.wpos_, 0);
        }
        return *this;
    }
    uint8_t* GetBasePointer() { return buffer_.data(); }
    uint8_t* GetReadPointer() { return buffer_.data() + rpos_; }
    uint8_t* GetWritePointer() { return buffer_.data() + wpos_; }
    void ReadCompleted(std::size_t size) { rpos_ += size; }
    void WriteCompleted(std::size_t size) { wpos_ += size; }
    std::size_t GetBufferSize() const { return buffer_.size(); }
    std::size_t GetUsedSize() const { return wpos_ - rpos_; }
    std::size_t GetFreeSize() const { return buffer_.size() - wpos_; }
    void Normalize() {
        if (rpos_ > 0) {
            std::memmove(buffer_.data(), buffer_.data() + rpos_, wpos_ - rpos_);
            wpos_ -= rpos_;
            rpos_ = 0;
        }
    }
    void EnsureFreeSize(std::size_t size) {
        if (GetBufferSize() - GetUsedSize() < size) {
            Normalize();
            buffer_.resize(buffer_.size() + std::max(size, buffer_.size() / 2));
        }
    }
    // Windows iocp boost.asio
    void Write(const uint8_t* data, std::size_t size) {
        EnsureFreeSize(size);
        std::memcpy(GetWritePointer(), data, size);
        WriteCompleted(size);
    }

    ssize_t Recv(int fd, int* err) {
        char extra_buf[65536];  // 64KB
        /**
         *struct iovec {
         *    ptr_t iov_base; // iov_base指向的缓冲区存放的是readv所接收的数据或是writev将要发送的数据
         *    size_t iov_len; // iov_len在各种情况下分别确定了接收的最大长度以及实际写入的长度
         *};
         */
        struct iovec vec[2];
        vec[0].iov_base = GetWritePointer();
        vec[0].iov_len = GetFreeSize();
        vec[1].iov_base = extra_buf;
        vec[1].iov_len = sizeof(extra_buf);

        std::size_t n = readv(fd, vec, 2);
        if (n < 0) {
            *err = errno;
            return n;
        } else if (n == 0) {
            *err = EBADE;
            return 0;
        } else if (n <= GetFreeSize()) {
            WriteCompleted(n);
            return n;
        } else {
            WriteCompleted(GetFreeSize());
            std::size_t extra_size = n - GetFreeSize();
            Write(reinterpret_cast<uint8_t*>(extra_buf), extra_size);
            return n;
        }
    }

private:
    std::vector<uint8_t> buffer_;
    std::size_t rpos_;
    std::size_t wpos_;
};
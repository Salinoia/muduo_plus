#pragma once

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

// 网络库底层的缓冲区类型定义
class Buffer {
public:
    static const size_t kCheapPrepend = 8;  // 初始预留的prependabel空间大小
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initalSize = kInitialSize) : buffer_(kCheapPrepend + initalSize), readerIndex_(kCheapPrepend), writerIndex_(kCheapPrepend) {}

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // 返回缓冲区中可读数据的起始地址
    const char* peek() const { return begin() + readerIndex_; }
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            readerIndex_ += len;  // 说明应用只读取了可读缓冲区数据的一部分，就是len长度 还剩下readerIndex+=len到writerIndex_的数据未读
        } else {  // len == readableBytes()
            retrieveAll();
        }
    }
    void retrieveAll() {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        retrieve(len);  // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        return result;
    }

    // buffer_.size - writerIndex_
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);  // 扩容
        }
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区当中
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    // 从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);

private:
    // ==== 核心数据存储 ====
    std::vector<char> buffer_;  // 数据缓冲区（核心成员，应置顶）

    // ==== 读写位置索引 ====
    size_t readerIndex_;  // 读位置指针（与buffer_强相关）
    size_t writerIndex_;  // 写位置指针

    // ==== 内部方法 ====
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    /**
     * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
     * | kCheapPrepend | reader ｜          len          |
     **/
    void makeSpace(size_t len) {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend) {  // 也就是说 len > xxx前面剩余的空间 + writer的部分
            buffer_.resize(writerIndex_ + len);
        } else {  // 这里说明 len <= xxx + writer 把reader搬到从xxx开始 使得xxx后面是一段连续空间
            size_t readable = readableBytes();  // readable = reader的长度
            // 将当前缓冲区中从readerIndex_到writerIndex_的数据
            // 拷贝到缓冲区起始位置kCheapPrepend处，以便腾出更多的可写空间
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
};

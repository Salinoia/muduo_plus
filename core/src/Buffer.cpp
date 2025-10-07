#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

// 从文件描述符读取数据到缓冲区（LT模式）
// 使用readv实现高效读取：优先使用Buffer空间，不足时暂存到栈空间再以append的方式追加到buffer_
ssize_t Buffer::readFd(int fd, int* saveErrno) {
    char extrabuf[65536] = {0};  // 64KB栈空间备用缓冲区

    /**
     *struct iovec {
     *    ptr_t iov_base; // iov_base指向的缓冲区存放的是readv所接收的数据或是writev将要发送的数据
     *    size_t iov_len; // iov_len在各种情况下分别确定了接收的最大长度以及实际写入的长度
     *};
     */
    struct iovec vec[2];
    const size_t writable = writableBytes();

    // 设置iovec结构
    // 第一块缓冲区，指向可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区，指向栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    // 根据剩余空间决定使用1个还是2个缓冲区
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *saveErrno = errno;
    } else if (n <= writable) {
        writerIndex_ += n;  // 全部数据存入Buffer
    } else {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);  // 追加栈空间数据
    }
    return n;
}

// 将缓冲区数据写入文件描述符
ssize_t Buffer::writeFd(int fd, int* saveErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *saveErrno = errno;
    }
    return n;
}

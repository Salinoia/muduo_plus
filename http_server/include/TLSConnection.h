#pragma once

#include <openssl/ssl.h>

#include <memory>

#include "Buffer.h"
#include "NonCopyable.h"
#include "TLSContext.h"
#include "TcpConnection.h"

// 添加消息回调函数类型定义
using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*, Timestamp)>;

class TLSConnection : NonCopyable {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using BufferPtr = Buffer*;

    TLSConnection(const TcpConnectionPtr& conn, TLSContext* ctx);
    ~TLSConnection();

    void startHandshake();
    void send(const void* data, size_t len);
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, Timestamp time);
    bool isHandshakeCompleted() const { return state_ == TLSState::ESTABLISHED; }
    Buffer* getDecryptedBuffer() { return &decryptedBuffer_; }
    // TLS BIO 操作回调
    static int bioWrite(BIO* bio, const char* data, int len);
    static int bioRead(BIO* bio, char* data, int len);
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);
    // 设置消息回调函数
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

private:
    void handleHandshake();
    void onEncrypted(const char* data, size_t len);
    void onDecrypted(const char* data, size_t len);
    TLSError getLastError(int ret);
    void handleError(TLSError error);
    void flushWriteBio();

private:
    SSL* ssl_;  // TLS 连接
    TLSContext* ctx_;  // TLS 上下文
    TcpConnectionPtr conn_;  // TCP 连接
    TLSState state_;  // TLS 状态
    BIO* readBio_;  // 网络数据 -> TLS
    BIO* writeBio_;  // TLS -> 网络数据
    Buffer readBuffer_;  // 读缓冲区
    Buffer writeBuffer_;  // 写缓冲区
    Buffer decryptedBuffer_;  // 解密后的数据
    MessageCallback messageCallback_;  // 消息回调
};


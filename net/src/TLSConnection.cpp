#include "TLSConnection.h"

#include <openssl/err.h>

#include "LogMacros.h"

// 自定义 BIO 方法
[[maybe_unused]] static BIO_METHOD* createCustomBioMethod() {
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_write(method, TLSConnection::bioWrite);
    BIO_meth_set_read(method, TLSConnection::bioRead);
    BIO_meth_set_ctrl(method, TLSConnection::bioCtrl);
    return method;
}

TLSConnection::TLSConnection(const TcpConnectionPtr& conn, TLSContext* ctx) :
    ssl_(nullptr), ctx_(ctx), conn_(conn), state_(TLSState::HANDSHAKE), readBio_(nullptr), writeBio_(nullptr), messageCallback_(nullptr) {
    // 创建 TLS 对象
    ssl_ = SSL_new(ctx_->getNativeHandle());
    if (!ssl_) {
        LOG_ERROR("Failed to create TLS object: {}", ERR_error_string(ERR_get_error(), nullptr));
        return;
    }

    // 创建 BIO
    readBio_ = BIO_new(BIO_s_mem());
    writeBio_ = BIO_new(BIO_s_mem());

    if (!readBio_ || !writeBio_) {
        LOG_ERROR("Failed to create BIO objects");
        SSL_free(ssl_);
        ssl_ = nullptr;
        return;
    }

    SSL_set_bio(ssl_, readBio_, writeBio_);
    SSL_set_accept_state(ssl_);  // 设置为服务器模式

    // 设置 TLS 选项
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // 设置连接回调
    conn_->setMessageCallback(std::bind(&TLSConnection::onRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

TLSConnection::~TLSConnection() {
    if (ssl_) {
        SSL_free(ssl_);  // 这会同时释放 BIO
    }
}

void TLSConnection::startHandshake() {
    SSL_set_accept_state(ssl_);
    handleHandshake();
}

void TLSConnection::send(const void* data, size_t len) {
    if (state_ != TLSState::ESTABLISHED) {
        LOG_ERROR("Cannot send data before TLS handshake is complete");
        return;
    }

    int written = SSL_write(ssl_, data, static_cast<int>(len));
    if (written <= 0) {
        int err = SSL_get_error(ssl_, written);
        LOG_ERROR("SSL_write failed: {}", ERR_error_string(err, nullptr));
        return;
    }

    char buf[4096];
    int pending;
    while ((pending = BIO_pending(writeBio_)) > 0) {
        int bytes = BIO_read(writeBio_, buf, std::min(pending, static_cast<int>(sizeof(buf))));
        if (bytes > 0) {
            conn_->send(buf, bytes);
        }
    }
}

void TLSConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, Timestamp time) {
    if (state_ == TLSState::HANDSHAKE) {
        // 将数据写入 BIO
        BIO_write(readBio_, buf->peek(), static_cast<int>(buf->readableBytes()));
        buf->retrieve(buf->readableBytes());
        handleHandshake();
        return;
    } else if (state_ == TLSState::ESTABLISHED) {
        // 解密数据
        char decryptedData[4096];
        int ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData));
        if (ret > 0) {
            // 创建新的 Buffer 存储解密后的数据
            Buffer decryptedBuffer;
            decryptedBuffer.append(decryptedData, ret);

            // 调用上层回调处理解密后的数据
            if (messageCallback_) {
                messageCallback_(conn, &decryptedBuffer, time);
            }
        }
    }
}

void TLSConnection::handleHandshake() {
    int ret = SSL_do_handshake(ssl_);

    if (ret == 1) {
        state_ = TLSState::ESTABLISHED;
        LOG_INFO("TLS handshake completed successfully");
        LOG_INFO("Using cipher: {}", SSL_get_cipher(ssl_));
        LOG_INFO("Protocol version: {}", SSL_get_version(ssl_));

        // 握手完成后，确保设置了正确的回调
        if (!messageCallback_) {
            LOG_WARN("No message callback set after TLS handshake");
        }
        return;
    }

    int err = SSL_get_error(ssl_, ret);
    switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        // 正常的握手过程，需要继续
        break;

    default: {
        // 获取详细的错误信息
        char errBuf[256];
        unsigned long errCode = ERR_get_error();
        ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
        LOG_ERROR("TLS handshake failed: {}", errBuf);
        conn_->shutdown();  // 关闭连接
        break;
    }
    }
}

void TLSConnection::onEncrypted(const char* data, size_t len) {
    conn_->send(data, len);
}

void TLSConnection::onDecrypted(const char* data, size_t len) {
    decryptedBuffer_.append(data, len);
}

TLSError TLSConnection::getLastError(int ret) {
    int err = SSL_get_error(ssl_, ret);
    switch (err) {
    case SSL_ERROR_NONE:
        return TLSError::NONE;
    case SSL_ERROR_WANT_READ:
        return TLSError::WANT_READ;
    case SSL_ERROR_WANT_WRITE:
        return TLSError::WANT_WRITE;
    case SSL_ERROR_SYSCALL:
        return TLSError::SYSCALL;
    case SSL_ERROR_SSL:
        return TLSError::TLS;
    default:
        return TLSError::UNKNOWN;
    }
}

void TLSConnection::handleError(TLSError error) {
    switch (error) {
    case TLSError::WANT_READ:
    case TLSError::WANT_WRITE:
        // 需要等待更多数据或写入缓冲区可用
        break;
    case TLSError::TLS:
    case TLSError::SYSCALL:
    case TLSError::UNKNOWN:
        LOG_ERROR("TLS error occurred: ", ERR_error_string(ERR_get_error(), nullptr));
        state_ = TLSState::ERROR;
        conn_->shutdown();
        break;
    default:
        break;
    }
}

void TLSConnection::flushWriteBio() {
    char buffer[4096];
    int pending = 0;
    while ((pending = BIO_pending(writeBio_)) > 0) {
        int bytes = BIO_read(writeBio_, buffer, std::min(pending, static_cast<int>(sizeof(buffer))));
        if (bytes > 0) {
            conn_->send(buffer, bytes);
        }
    }
}

int TLSConnection::bioWrite(BIO* bio, const char* data, int len) {
    TLSConnection* conn = static_cast<TLSConnection*>(BIO_get_data(bio));
    if (!conn || !conn->conn_)
        return -1;

    conn->conn_->send(data, len);
    return len;
}

int TLSConnection::bioRead(BIO* bio, char* data, int len) {
    TLSConnection* conn = static_cast<TLSConnection*>(BIO_get_data(bio));
    if (!conn)
        return -1;

    size_t readable = conn->readBuffer_.readableBytes();
    if (readable == 0) {
        return -1;  // 无数据可读
    }

    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_.peek(), toRead);
    conn->readBuffer_.retrieve(toRead);
    return static_cast<int>(toRead);
}

long TLSConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr) {
    switch (cmd) {
    case BIO_CTRL_FLUSH:
        return 1;
    default:
        return 0;
    }
}

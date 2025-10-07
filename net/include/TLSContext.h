#pragma once
#include <openssl/ssl.h>

#include <memory>

#include "NonCopyable.h"
#include "TLSConfig.h"

class TLSContext : NonCopyable {
public:
    explicit TLSContext(const TLSConfig& config);
    ~TLSContext();

    bool initialize();
    SSL_CTX* getNativeHandle() { return ctx_; }

private:
    bool loadCertificates();
    bool setupProtocol();
    void setupSessionCache();
    static void handleSslError(const char* msg);

private:
    SSL_CTX* ctx_;  // TLS上下文
    TLSConfig config_;  // TLS配置
};

#pragma once
#include <string>

// TLS/TLS 协议版本
enum class TLSVersion { TLS_1_0, TLS_1_1, TLS_1_2, TLS_1_3 };

// TLS 错误类型
enum class TLSError { NONE, WANT_READ, WANT_WRITE, SYSCALL, TLS, UNKNOWN };

// TLS 状态
enum class TLSState { HANDSHAKE, ESTABLISHED, SHUTDOWN, ERROR };

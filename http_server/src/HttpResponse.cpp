#include "http/HttpResponse.h"

#include <cstdio>
#include <cstring>

#include "Buffer.h"

HttpResponse::HttpResponse(bool close) : statusCode_(kUnknown), closeConnection_(close) {}

void HttpResponse::appendToBuffer(Buffer* output) const {
    char buf[64];
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode_);
    output->append(buf, strlen(buf));
    output->append(statusMessage_.c_str(), statusMessage_.size());
    output->append("\r\n", 2);

    if (closeConnection_) {
        output->append("Connection: close\r\n", 19);
    } else {
        snprintf(buf, sizeof buf, "Content-Length: %zu\r\n", body_.size());
        output->append(buf, strlen(buf));
        output->append("Connection: Keep-Alive\r\n", 24);
    }

    for (const auto& header : headers_) {
        output->append(header.first.c_str(), header.first.size());
        output->append(": ", 2);
        output->append(header.second.c_str(), header.second.size());
        output->append("\r\n", 2);
    }

    output->append("\r\n", 2);
    output->append(body_.c_str(), body_.size());
}

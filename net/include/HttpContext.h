#pragma once

#include "HttpRequest.h"

class Buffer;
class Timestamp;

class HttpContext {
public:
    enum HttpRequestParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext() : state_(kExpectRequestLine) {}

    bool parseRequest(Buffer* buf, Timestamp receiveTime);

    bool gotAll() const { return state_ == kGotAll; }
    void reset();

    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }

private:
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;
};

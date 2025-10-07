#include "HttpContext.h"

#include <algorithm>

#include "Buffer.h"
#include "Timestamp.h"

namespace {
const char kCRLF[] = "\r\n";
}

void HttpContext::reset() {
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
}
bool HttpContext::processRequestLine(const char* begin, const char* end) {
    bool succeed = false;
    const char* start = begin;
    const char* space = std::find(start, end, ' ');

    if (space != end && request_.setMethod(start, space)) {
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end) {
            const char* question = std::find(start, space, '?');
            if (question != space) {
                request_.setPath(start, question);
                request_.setQueryParameters(question + 1, space);
            } else {
                request_.setPath(start, space);
            }

            start = space + 1;
            succeed = ((end - start == 8) && std::equal(start, end - 1, "HTTP/1."));
            if (succeed) {
                if (*(end - 1) == '1') {
                    request_.setVersion("HTTP/1.1");
                } else if (*(end - 1) == '0') {
                    request_.setVersion("HTTP/1.0");
                } else {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime) {
    bool ok = true;
    bool hasMore = true;

    // 匿名 lambda：在 buf 中查找一行
    auto findLine = [&](const char*& start, const char*& crlf) -> bool {
        size_t readable = buf->readableBytes();
        start = buf->peek();
        crlf = std::search(start, start + readable, kCRLF, kCRLF + 2);
        return crlf != start + readable;
    };

    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            const char* start;
            const char* crlf;
            if (findLine(start, crlf)) {
                ok = processRequestLine(start, crlf);
                if (ok) {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieve(crlf + 2 - start);
                    state_ = kExpectHeaders;
                } else {
                    ok = false;
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectHeaders) {
            const char* start;
            const char* crlf;
            if (findLine(start, crlf)) {
                const char* colon = std::find(start, crlf, ':');
                if (colon != crlf) {
                    request_.addHeader(start, colon, crlf);
                } else if (start == crlf) {
                    if (request_.method() == HttpRequest::kPost || request_.method() == HttpRequest::kPut) {
                        std::string contentLength = request_.getHeader("Content-Length");
                        if (!contentLength.empty()) {
                            request_.setContentLength(std::stoul(contentLength));
                            if (request_.contentLength() > 0) {
                                state_ = kExpectBody;
                            } else {
                                state_ = kGotAll;
                                hasMore = false;
                            }
                        } else {
                            ok = false;  // POST/PUT 缺少 Content-Length
                            hasMore = false;
                        }
                    } else {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                } else {
                    ok = false;  // Header 格式错误
                    hasMore = false;
                }
                buf->retrieve(crlf + 2 - start);
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectBody) {
            if (buf->readableBytes() < request_.contentLength()) {
                hasMore = false;  // body 未收全
                return true;
            }

            std::string body(buf->peek(), buf->peek() + request_.contentLength());
            request_.setBody(body);
            buf->retrieve(request_.contentLength());

            state_ = kGotAll;
            hasMore = false;
        } else {
            hasMore = false;
        }
    }
    return ok;
}

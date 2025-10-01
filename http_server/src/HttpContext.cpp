#include "http/HttpContext.h"

#include <algorithm>

#include "Buffer.h"
#include "TimeStamp.h"

namespace {
const char kCRLF[] = "\r\n";
}

void HttpContext::reset() {
    state_ = kExpectRequestLine;
    HttpRequest dummy;
    request_.swap(dummy);
}

bool HttpContext::processRequestLine(const char* begin, const char* end) {
    const char* start = begin;
    const char* space = std::find(start, end, ' ');
    if (space == end)
        return false;
    if (!request_.setMethod(start, space))
        return false;
    start = space + 1;
    space = std::find(start, end, ' ');
    const char* question = std::find(start, space, '?');
    if (question != space) {
        request_.setPath(start, question);
        request_.setQuery(question + 1, space);
    } else {
        request_.setPath(start, space);
    }
    start = space + 1;
    if (end - start == 8 && std::equal(start, end - 1, "HTTP/1.")) {
        if (*(end - 1) == '1') {
            request_.setVersion(HttpRequest::kHttp11);
        } else if (*(end - 1) == '0') {
            request_.setVersion(HttpRequest::kHttp10);
        } else {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

bool HttpContext::parseRequest(Buffer* buf, TimeStamp /*receiveTime*/) {
    bool ok = true;
    bool hasMore = true;
    while (hasMore) {
        if (state_ == kExpectRequestLine) {
            size_t readable = buf->readableBytes();
            const char* start = buf->peek();
            const char* crlf = std::search(start, start + readable, kCRLF, kCRLF + 2);
            if (crlf != start + readable) {
                if (processRequestLine(start, crlf)) {
                    state_ = kExpectHeaders;
                    buf->retrieve(crlf + 2 - start);
                } else {
                    ok = false;
                    hasMore = false;
                }
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectHeaders) {
            size_t readable = buf->readableBytes();
            const char* start = buf->peek();
            const char* crlf = std::search(start, start + readable, kCRLF, kCRLF + 2);
            if (crlf != start + readable) {
                const char* colon = std::find(start, crlf, ':');
                if (colon != crlf) {
                    request_.addHeader(start, colon, crlf);
                } else {
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieve(crlf + 2 - start);
            } else {
                hasMore = false;
            }
        } else if (state_ == kExpectBody) {
            // not implemented
            state_ = kGotAll;
            hasMore = false;
        } else {
            hasMore = false;
        }
    }
    return ok;
}

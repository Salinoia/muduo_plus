#include "HttpRequest.h"

#include <algorithm>
#include <cassert>
#include <cctype>

bool HttpRequest::setMethod(const char* start, const char* end) {
    std::string m(start, end);
    if (m == "GET")
        method_ = kGet;
    else if (m == "POST")
        method_ = kPost;
    else if (m == "HEAD")
        method_ = kHead;
    else if (m == "PUT")
        method_ = kPut;
    else if (m == "DELETE")
        method_ = kDelete;
    else if (m == "OPTIONS")
        method_ = kOptions;
    else
        method_ = kInvalid;
    return method_ != kInvalid;
}

const char* HttpRequest::methodString() const {
    switch (method_) {
    case kGet:
        return "GET";
    case kPost:
        return "POST";
    case kHead:
        return "HEAD";
    case kPut:
        return "PUT";
    case kDelete:
        return "DELETE";
    case kOptions:
        return "OPTIONS";
    default:
        return "UNKNOWN";
    }
}

HttpRequest::Version HttpRequest::setVersion(HttpRequest::Version v) {
    switch (v) {
    case Version::kHttp10:
        version_ = "HTTP/1.0";
        break;
    case Version::kHttp11:
        version_ = "HTTP/1.1";
        break;
    case Version::kHttp2:
        version_ = "HTTP/2.0";
        break;
    case Version::kHttp3:
        version_ = "HTTP/3.0";
        break;
    default:
        version_ = "Unknown";
        break;
    }
    return v;
}

HttpRequest::Version HttpRequest::versionEnum() const {
    if (version_ == "HTTP/1.0")
        return Version::kHttp10;
    else if (version_ == "HTTP/1.1")
        return Version::kHttp11;
    else if (version_ == "HTTP/2.0")
        return Version::kHttp2;
    else if (version_ == "HTTP/3.0")
        return Version::kHttp3;
    else
        return Version::kUnknown;
}

void HttpRequest::setQueryParameters(const char* start, const char* end) {
    std::string argumentStr(start, end);
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;

    // 按照 '&' 分割多个参数
    while ((pos = argumentStr.find('&', prev)) != std::string::npos) {
        std::string pair = argumentStr.substr(prev, pos - prev);
        auto equalPos = pair.find('=');
        if (equalPos != std::string::npos) {
            queryParameters_[pair.substr(0, equalPos)] = pair.substr(equalPos + 1);
        }
        prev = pos + 1;
    }
    std::string lastPair = argumentStr.substr(prev);
    auto equalPos = lastPair.find('=');
    if (equalPos != std::string::npos) {
        queryParameters_[lastPair.substr(0, equalPos)] = lastPair.substr(equalPos + 1);
    }
}

std::string HttpRequest::getQueryParameter(const std::string& key) const {
    auto it = queryParameters_.find(key);
    return it != queryParameters_.end() ? it->second : "";
}

void HttpRequest::addHeader(const char* start, const char* colon, const char* end) {
    std::string field(start, colon);
    ++colon;
    while (colon < end && isspace(*colon))
        ++colon;
    std::string value(colon, end);
    while (!value.empty() && isspace(value.back()))
        value.pop_back();
    headers_[field] = value;
}

std::string HttpRequest::getHeader(const std::string& field) const {
    auto it = headers_.find(field);
    return it != headers_.end() ? it->second : "";
}

void HttpRequest::swap(HttpRequest& that) {
    std::swap(method_, that.method_);
    std::swap(version_, that.version_);
    path_.swap(that.path_);
    query_.swap(that.query_);
    headers_.swap(that.headers_);
    pathParameters_.swap(that.pathParameters_);
    queryParameters_.swap(that.queryParameters_);
    content_.swap(that.content_);
    std::swap(contentLength_, that.contentLength_);
    std::swap(receiveTime_, that.receiveTime_);
}

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

#include "Timestamp.h"  // 轻量级时间戳类，无需 pimpl

class HttpRequest {
public:
    enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete, kOptions };
    enum class Version { kUnknown, kHttp10, kHttp11, kHttp2, kHttp3 };

    HttpRequest() : method_(kInvalid), version_("Unknown"), contentLength_(0) {}

    // 时间
    void setReceiveTime(Timestamp t) { receiveTime_ = t; }
    Timestamp receiveTime() const { return receiveTime_; }

    // HTTP 方法
    bool setMethod(const char* start, const char* end);
    Method method() const { return method_; }
    const char* methodString() const;

    // HTTP 版本
    void setVersion(const std::string& v) { version_ = v; }
    Version setVersion(Version v);
    const std::string& versionString() const { return version_; }
    Version versionEnum() const;
    // 路径
    void setPath(const char* start, const char* end) { path_.assign(start, end); }
    const std::string& path() const { return path_; }

    // 路径参数
    void setPathParameter(const std::string& key, const std::string& value) { pathParameters_[key] = value; }
    std::string getPathParameter(const std::string& key) const {
        auto it = pathParameters_.find(key);
        return it != pathParameters_.end() ? it->second : "";
    }

    // 查询参数
    void setQuery(const char* start, const char* end) { query_.assign(start, end); }
    const std::string& query() const { return query_; }

    void setQueryParameters(const char* start, const char* end);
    std::string getQueryParameter(const std::string& key) const;

    // 请求头
    void addHeader(const char* start, const char* colon, const char* end);
    std::string getHeader(const std::string& field) const;
    const std::map<std::string, std::string>& headers() const { return headers_; }

    // 请求体
    void setBody(const std::string& body) { content_ = body; }
    void setBody(const char* start, const char* end) {
        if (end >= start) {
            content_.assign(start, end - start);
        }
    }
    const std::string& body() const { return content_; }

    void setContentLength(std::size_t length) { contentLength_ = length; }
    std::size_t contentLength() const { return contentLength_; }

    // 工具函数
    void swap(HttpRequest& that);

private:
    Method method_;  // 请求方法
    std::string version_;  // HTTP 版本
    std::string path_;  // 请求路径
    std::unordered_map<std::string, std::string> pathParameters_;  // 路径参数
    std::string query_;  // 原始查询串
    std::unordered_map<std::string, std::string> queryParameters_;  // 查询参数
    Timestamp receiveTime_;  // 接收时间
    std::map<std::string, std::string> headers_;  // 请求头
    std::string content_;  // 请求体
    std::size_t contentLength_;  // 请求体长度
};

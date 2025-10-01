#pragma once

#include <map>
#include <string>

class HttpRequest {
public:
    enum Method {
        kInvalid,
        kGet,
        kPost,
        kHead,
        kPut,
        kDelete,
    };

    enum Version {
        kUnknown,
        kHttp10,
        kHttp11,
    };

    HttpRequest() : method_(kInvalid), version_(kUnknown) {}

    void setVersion(Version v) { version_ = v; }
    Version version() const { return version_; }

    bool setMethod(const char* start, const char* end);
    Method method() const { return method_; }
    const char* methodString() const;

    void setPath(const char* start, const char* end) { path_.assign(start, end); }
    const std::string& path() const { return path_; }

    void setQuery(const char* start, const char* end) { query_.assign(start, end); }
    const std::string& query() const { return query_; }

    void addHeader(const char* start, const char* colon, const char* end);
    std::string getHeader(const std::string& field) const;

    const std::map<std::string, std::string>& headers() const { return headers_; }

    void swap(HttpRequest& that);

private:
    Method method_;
    Version version_;
    std::string path_;
    std::string query_;
    std::map<std::string, std::string> headers_;
};

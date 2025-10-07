#pragma once

#include <map>
#include <string>

class Buffer;

class HttpResponse {
public:
    enum HttpStatusCode {
        kUnknown,
        // 2xx 成功，表示请求已成功被服务器接收、理解、并接受
        k200Ok = 200,
        k204NoContent = 204,
        k206ParitialContent = 206,
        // 3xx 重定向，表示要完成请求必须进行更进一步的操作
        k301MovedPermanently = 301,
        k302Found = 302,
        k304NotModified = 304,
        // 4xx 客户端错误，表示请求可能出错，妨碍了服务器的处理
        k400BadRequest = 400,
        k403Forbidden = 403,
        k404NotFound = 404,
        // 5xx 服务器错误，表示服务器在处理请求的过程中发生了错误
        k500InternalServerError = 500,
        k501NotImplemented = 501,
        k502BadGateway = 502,
        k503ServiceUnavailable = 503,
    };

    explicit HttpResponse(bool close);

    void setVersion(const std::string& version) { httpVersion_ = version; }
    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    void setStatusMessage(const std::string& message) { statusMessage_ = message; }

    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void setContentType(const std::string& contentType) { addHeader("Content-Type", contentType); }
    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }

    void setBody(const std::string& body) { body_ = body; }

    void appendToBuffer(Buffer* output) const;
    void setStatusLine(const std::string& version, HttpStatusCode statusCode, const std::string& statusMessage);
    // void setErrorHeader() {}

private:
    std::string httpVersion_;
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    bool closeConnection_;
};

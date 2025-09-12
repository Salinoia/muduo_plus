#pragma once

#include <map>
#include <string>

class Buffer;

class HttpResponse {
public:
    enum HttpStatusCode {
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k401Unauthorized = 401,
        k403Forbidden = 403,
        k400BadRequest = 400,
        k404NotFound = 404,
    };

    explicit HttpResponse(bool close);

    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    void setStatusMessage(const std::string& message) { statusMessage_ = message; }
    HttpStatusCode statusCode() const { return statusCode_; }

    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void setContentType(const std::string& contentType) { addHeader("Content-Type", contentType); }
    void addHeader(const std::string& key, const std::string& value) { headers_[key] = value; }

    void setBody(const std::string& body) { body_ = body; }

    void appendToBuffer(Buffer* output) const;

private:
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    bool closeConnection_;
};

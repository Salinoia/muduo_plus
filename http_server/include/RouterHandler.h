#pragma once
#include <memory>
#include <string>

class HttpRequest;
class HttpResponse;

class RouterHandler {
public:
    virtual ~RouterHandler() = default;
    virtual void handle(const HttpRequest& req, HttpResponse* resp) = 0;
};

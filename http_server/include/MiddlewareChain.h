#pragma once

#include <memory>
#include <vector>

#include "Middleware.h"

class MiddlewareChain {
public:
    void addMiddleware(std::shared_ptr<Middleware> middleware);
    void processBefore(HttpRequest& request);
    void processAfter(HttpResponse& response);

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};

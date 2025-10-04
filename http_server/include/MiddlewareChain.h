#pragma once
#include <memory>
#include <vector>

#include "Middleware.h"

/**
 * @brief 中间件链（由 HttpServer 调用）
 *
 * 内部维护一组顺序执行的中间件。
 * handle() 遍历所有中间件，若任意中间件返回 false，则立即中断。
 */
class MiddlewareChain {
public:
    void addMiddleware(std::shared_ptr<Middleware> m) { middlewares_.emplace_back(std::move(m)); }

    /**
     * @brief 顺序执行所有中间件
     * @return bool 是否继续后续处理
     */
    bool handle(HttpRequest& req, HttpResponse& resp) {
        for (auto& m : middlewares_) {
            if (!m->handle(req, resp)) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};

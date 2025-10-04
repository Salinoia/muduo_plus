
#pragma once
#include <memory>

class HttpRequest;
class HttpResponse;

/**
 * @brief 中间件基类（顺序链执行）
 *
 * 在框架层，外部驱动顺序中间件链的调用策略远优于责任链递归推进
 *
 * 框架由 HttpServer 调用 MiddlewareChain::handle(req, resp)
 * 顺序执行所有中间件。
 *
 * - 若返回 false，则中断链（例如认证失败、CORS 拒绝等）。
 * - 若返回 true，则继续执行后续中间件。
 *
 * 每个中间件都可以直接修改 HttpRequest / HttpResponse。
 */
class Middleware {
public:
    virtual ~Middleware() = default;

    /**
     * @brief 处理请求与响应
     * @param request  当前请求
     * @param response 当前响应
     * @return bool 若返回 false，则中断中间件链
     */
    virtual bool handle(HttpRequest& request, HttpResponse& response) = 0;
};
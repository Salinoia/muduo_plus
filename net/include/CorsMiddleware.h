#pragma once

#include <memory>
#include <string>
#include <vector>

#include "CorsConfig.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Middleware.h"

/**
 * @brief CORS（跨域资源共享）中间件（与 Middleware::handle 对齐）
 *
 * 处理逻辑：
 * - 无 Origin：放行（返回 true）
 * - Origin 不允许：设置 403 并中断（返回 false）
 * - 预检（OPTIONS）：返回 200 + 预检头并中断（返回 false）
 * - 普通请求：注入 CORS 头并继续（返回 true）
 */
class CorsMiddleware final : public Middleware {
public:
    explicit CorsMiddleware(CorsConfig config = CorsConfig::defaultConfig()) noexcept;

    // 返回 false 表示中断后续中间件/路由
    bool handle(HttpRequest& request, HttpResponse& response) override;

private:
    bool isOriginAllowed(const std::string& origin) const noexcept;
    void handlePreflightRequest(const HttpRequest& request, HttpResponse& response);
    void addCorsHeaders(HttpResponse& response, const std::string& origin);

    static std::string join(const std::vector<std::string>& v, const std::string& delim);

private:
    CorsConfig config_;
};

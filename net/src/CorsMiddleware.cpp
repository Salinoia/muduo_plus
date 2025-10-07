#include "CorsMiddleware.h"

#include <algorithm>

CorsMiddleware::CorsMiddleware(CorsConfig config) noexcept : config_(std::move(config)) {}

bool CorsMiddleware::handle(HttpRequest& request, HttpResponse& response) {
    const std::string origin = request.getHeader("Origin");

    // 1) 非跨域请求（无 Origin）：直接放行
    if (origin.empty()) {
        return true;
    }

    // 2) 非白名单来源：拒绝并中断
    if (!isOriginAllowed(origin)) {
        response.setStatusCode(HttpResponse::k403Forbidden);
        response.setStatusMessage("Forbidden");
        response.setContentType("text/plain; charset=utf-8");
        response.setBody("CORS origin denied");
        return false;
    }

    // 3) 预检请求（OPTIONS）：返回 200 + 预检响应头并中断
    if (request.method() == HttpRequest::kOptions) {
        handlePreflightRequest(request, response);
        return false;
    }

    // 4) 普通跨域请求：添加 CORS 头并继续链条
    addCorsHeaders(response, origin);
    return true;
}

bool CorsMiddleware::isOriginAllowed(const std::string& origin) const noexcept {
    if (config_.allowAllOrigins)
        return true;
    return std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end();
}

void CorsMiddleware::handlePreflightRequest(const HttpRequest& request, HttpResponse& response) {
    response.setStatusCode(HttpResponse::k200Ok);
    response.setStatusMessage("OK");

    const std::string origin = request.getHeader("Origin");
    addCorsHeaders(response, origin);

    // 允许的方法
    if (!config_.allowedMethods.empty()) {
        response.addHeader("Access-Control-Allow-Methods", join(config_.allowedMethods, ", "));
    }

    // 允许的请求头
    if (!config_.allowedHeaders.empty()) {
        response.addHeader("Access-Control-Allow-Headers", join(config_.allowedHeaders, ", "));
    } else {
        // 若未配置，尽量回显请求头（可按需保守关闭）
        const std::string reqHdrs = request.getHeader("Access-Control-Request-Headers");
        if (!reqHdrs.empty()) {
            response.addHeader("Access-Control-Allow-Headers", reqHdrs);
        }
    }

    // 预检缓存时间
    if (config_.maxAge > 0) {
        response.addHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));
    }

    response.setBody("");
}

void CorsMiddleware::addCorsHeaders(HttpResponse& response, const std::string& origin) {
    // allowAllOrigins 且 允许凭证 时，不能用 "*"，应回显具体 Origin
    if (config_.allowAllOrigins && !config_.allowCredentials) {
        response.addHeader("Access-Control-Allow-Origin", "*");
    } else {
        response.addHeader("Access-Control-Allow-Origin", origin);
        response.addHeader("Vary", "Origin");  // 告诉缓存：按 Origin 区分
    }

    if (config_.allowCredentials) {
        response.addHeader("Access-Control-Allow-Credentials", "true");
    }

    if (!config_.exposedHeaders.empty()) {
        response.addHeader("Access-Control-Expose-Headers", join(config_.exposedHeaders, ", "));
    }
}

std::string CorsMiddleware::join(const std::vector<std::string>& v, const std::string& delim) {
    std::string result;
    for (size_t i = 0; i < v.size(); ++i) {
        result += v[i];
        if (i + 1 < v.size())
            result += delim;
    }
    return result;
}

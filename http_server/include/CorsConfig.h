#pragma once

#include <string>
#include <vector>

/**
 * @brief CORS 配置对象
 *
 * 包含所有与跨域策略相关的字段，可由外部加载或直接使用 defaultConfig()。
 *
 * 特点：
 * - 所有字段默认安全；
 * - 默认配置允许所有来源与常用方法；
 * - const-correctness；
 * - 支持直接复制或移动。
 */
class CorsConfig {
public:
    std::vector<std::string> allowedOrigins;  // 允许的来源
    std::vector<std::string> allowedMethods;  // 允许的方法
    std::vector<std::string> allowedHeaders;  // 允许的请求头
    std::vector<std::string> exposedHeaders;  // 允许暴露给浏览器的响应头
    bool allowCredentials{false};  // 是否允许凭证（Cookie、Authorization）
    bool allowAllOrigins{false};  // 是否直接放行所有来源（优先级高于 allowedOrigins）
    int maxAge{3600};  // 预检结果缓存时间（秒）

    CorsConfig() = default;
    CorsConfig(const CorsConfig&) = default;
    CorsConfig(CorsConfig&&) noexcept = default;
    CorsConfig& operator=(const CorsConfig&) = default;
    CorsConfig& operator=(CorsConfig&&) noexcept = default;

    /**
     * @brief 返回默认配置：常见的全开放跨域场景
     */
    static CorsConfig defaultConfig() noexcept {
        CorsConfig cfg;
        cfg.allowedOrigins = {"*"};
        cfg.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        cfg.allowedHeaders = {"Content-Type", "Authorization"};
        cfg.exposedHeaders = {};  // 默认无额外暴露
        cfg.allowCredentials = false;
        cfg.allowAllOrigins = true;
        cfg.maxAge = 3600;
        return cfg;
    }
};

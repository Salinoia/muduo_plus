#include "CorsMiddleware.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Logger.h"

CorsMiddleware::CorsMiddleware(const CorsConfig& config) : config_(config) {}

void CorsMiddleware::before(HttpRequest& request) {
    Logger::instance().debug("CorsMiddleware::before - Processing request");

    if (request.method() == HttpRequest::Method::kOptions) {
        Logger::instance().info("Processing CORS preflight request");
        HttpResponse response(false);
        handlePreflightRequest(request, response);
        throw response;
    }
}

void CorsMiddleware::after(HttpResponse& response) {
    Logger::instance().debug("CorsMiddleware::after - Processing response");

    if (!config_.allowedOrigins.empty()) {
        if (std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end()) {
            addCorsHeaders(response, "*");
        } else {
            addCorsHeaders(response, config_.allowedOrigins[0]);
        }
    }
}

bool CorsMiddleware::isOriginAllowed(const std::string& origin) const {
    return config_.allowedOrigins.empty() || std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end()
           || std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end();
}

void CorsMiddleware::handlePreflightRequest(const HttpRequest& request, HttpResponse& response) {
    const std::string& origin = request.getHeader("Origin");

    if (!isOriginAllowed(origin)) {
        Logger::instance().warn("Origin not allowed: " + origin);
        response.setStatusCode(HttpResponse::k403Forbidden);
        return;
    }

    addCorsHeaders(response, origin);
    response.setStatusCode(HttpResponse::k204NoContent);
    Logger::instance().info("Preflight request processed successfully");
}

void CorsMiddleware::addCorsHeaders(HttpResponse& response, const std::string& origin) {
    try {
        response.addHeader("Access-Control-Allow-Origin", origin);

        if (config_.allowCredentials) {
            response.addHeader("Access-Control-Allow-Credentials", "true");
        }

        if (!config_.allowedMethods.empty()) {
            response.addHeader("Access-Control-Allow-Methods", join(config_.allowedMethods, ", "));
        }

        if (!config_.allowedHeaders.empty()) {
            response.addHeader("Access-Control-Allow-Headers", join(config_.allowedHeaders, ", "));
        }

        response.addHeader("Access-Control-Max-Age", std::to_string(config_.maxAge));

        Logger::instance().debug("CORS headers added successfully");
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error adding CORS headers: ") + e.what());
    }
}

std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter) {
    std::ostringstream result;
    for (size_t i = 0; i < strings.size(); ++i) {
        if (i > 0)
            result << delimiter;
        result << strings[i];
    }
    return result.str();
}

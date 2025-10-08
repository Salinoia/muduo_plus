#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "app/OrderConfig.h"
#include "app/OrderApplication.h"
#include "EventLoop.h"
#include "InetAddress.h"

namespace fs = std::filesystem;

// --------------------------- 信号处理 ---------------------------
static EventLoop* g_loop = nullptr;

static void HandleSignal(int signo) {
    std::cerr << "\n[Signal] Caught " << signo << ", shutting down..." << std::endl;
    if (g_loop) g_loop->quit();  // 通知事件循环退出
}

// --------------------------- 配置路径解析 ---------------------------
static std::string ResolveConfigPath(int argc, char* argv[]) {
    std::string configPath;

    // 1. 命令行参数：--config /path/to/config.yaml
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
            break;
        }
    }

    // 2. 环境变量：ORDER_SERVER_CONFIG
    if (configPath.empty()) {
        if (const char* env = std::getenv("ORDER_SERVER_CONFIG")) {
            configPath = env;
        }
    }

    // 3. 可执行文件相对路径（适合从 bin/ 启动）
    if (configPath.empty()) {
        try {
            fs::path exeDir = fs::canonical(fs::path(argv[0])).parent_path();
            fs::path candidate = exeDir / "../apps/order_server/config/config.yaml";
            if (fs::exists(candidate))
                configPath = candidate.string();
        } catch (...) {
            // ignore
        }
    }

    // 4. 开发默认路径（适合直接在 apps/order_server 目录运行）
    if (configPath.empty()) {
        fs::path devPath = fs::current_path() / "config/config.yaml";
        if (fs::exists(devPath))
            configPath = devPath.string();
    }

    if (configPath.empty())
        throw std::runtime_error(
            "No valid configuration file found.\n"
            "Try: ./order_server --config /path/to/config.yaml\n"
            "Or set env: export ORDER_SERVER_CONFIG=/path/to/config.yaml");

    return fs::weakly_canonical(configPath).string();
}

// --------------------------- 主程序入口 ---------------------------
int main(int argc, char* argv[]) {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    try {
        std::string configPath = ResolveConfigPath(argc, argv);
        std::cout << "[Boot] Using config: " << configPath << std::endl;

        OrderServerOptions options = OrderServerOptions::FromConfig(configPath);

        EventLoop loop;
        g_loop = &loop;
        InetAddress listenAddr("0.0.0.0", 8080);
        OrderApplication app(&loop, listenAddr, options);

        std::cout << "[Boot] Starting service: " << options.serviceName << std::endl;
        app.start();

        loop.loop();  // 启动事件循环，让 HTTP/TCP 生效

        std::cout << "[Exit] Graceful shutdown complete." << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Fatal] " << e.what() << std::endl;
        return 1;
    }
}

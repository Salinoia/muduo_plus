# muduo_plus

## 项目概览
muduo_plus 从零实现了一个覆盖网络层、传输层、应用层的全栈 C++20 服务端样板：底层复刻并强化 Muduo 风格的 Reactor 事件循环；中间层整合 HTTP/TLS、Redis、MySQL、RabbitMQ；上层构建真实业务的订单服务。项目显示出如何在现代 C++ 中用标准库与精选开源库搭建可观测、可扩展的服务端基座。

## 架构亮点
- **事件循环自研到底**：`EventLoop`、`Channel`、`EPollPoller`、`TcpServer` 等核心组件完全手写，配合 `EventLoopThreadPool` 支撑多核扩展。
- **现代 C++ 语义**：广泛使用 `std::source_location`、`std::chrono`、`std::shared_ptr`/`std::unique_ptr`、结构化绑定、`std::format` 等 C++20 能力，让接口简洁且类型安全。
- **全链路中间件接入**：MySQL（mysql-connector-c++）、Redis（hiredis）、RabbitMQ（amqpcpp）、配置解析（yaml-cpp）、JSON 序列化（nlohmann_json）均与事件循环联动，减少阻塞。
- **业务分层可复用**：`apps/order_server` 以应用/App、领域/Domain、基础设施/Infra、接口/Interface 分层，提供复杂依赖组合、库存校验、缓存预热、消息驱动等真实场景。
- **工程化闭环**：模块化 CMake、Docker Compose、一键巡检脚本 `test.sh`，在学习框架的同时获取面向生产的工程体验。

## 目录结构速览
```
muduo_plus/
├── core/                    # 事件循环、Channel、Logger 等底层组件
│   ├── include/
│   └── src/
├── net/                     # HTTP/TLS 协议栈与中间件体系
│   ├── include/
│   └── src/
├── db/                      # MySQL 连接池、仓储工具
│   ├── include/
│   └── src/
├── cache/                   # Redis 客户端与连接池
├── mq/                      # AMQP-CPP 集成封装
├── apps/
│   └── order_server/
│       ├── app/             # 应用装配、配置解析
│       ├── domain/          # 领域服务（订单、库存）
│       ├── infra/           # DB/Cache/MQ 等基础设施实现
│       └── interface/       # HTTP 处理器、MQ 路由
├── tests/                   # echo/client 网络示例
├── docker-compose.yml       # 依赖服务编排
└── test.sh                  # 端到端巡检脚本
```

## 核心模块剖析
- **`core/` —— Reactor 内核**  
  - 以 epoll 为核心构建 `EPollPoller`，`Channel` 负责 fd 事件的生命周期管理。  
  - `EventLoop` 提供线程亲和的任务分发、`runInLoop`/`queueInLoop` 等跨线程调度。  
  - `EventLoopThread` 与 `EventLoopThreadPool` 通过 `std::thread` + 条件变量编排多 Reactor 模式。  
  - `Logger`/`LogMacros` 借助 `std::source_location` 自动注入文件行号，输出统一格式日志。  
  - 辅以 `Timestamp`、`Buffer`、`Thread` 等工具类，完整覆盖从 IO 多路复用到连接管理的基建。

- **`net/` —— HTTP & TLS 层**  
  - `HttpServer` 复用 `TcpServer`，支持 GET/POST 路由、正则路由与 Handler 对象化。  
  - `MiddlewareChain` 让鉴权、CORS、限流等可插拔；`SessionManager` 支持多种会话存储策略。  
  - `TLSContext`/`TLSConnection` 对 OpenSSL 做 RAII 封装，可无缝切换明文/TLS。  
  - `HttpRequest`/`HttpResponse` 提供简单的 Header/Body 操作与 JSON 友好接口，便于快速构建 REST API。

- **`db/` —— MySQL 数据访问层**  
  - `MySQLConn` 对 mysql-connector-c++ 做 RAII 包装，提供重试、超时、异常日志。  
  - `MySQLConnPool` 管理最小/最大连接数、空闲回收、阻塞队列，实现线程安全的池化访问。  
  - `SQLTask`/`BlockingQueue` 支持异步任务派发，`OrderRepository` 展示仓储模式整合 SQL 与领域模型。  
  - 使用 `nlohmann_json`/`yaml-cpp` 将配置转换为连接信息，方便扩展多数据源。

- **`cache/` —— Redis 缓存层**  
  - `RedisClient` 基于 hiredis，封装 `Connect/Get/Set/Del`，采用 `EnsureConnected` 保证断线重连。  
  - `RedisPool` 以 `std::queue<std::unique_ptr<RedisClient>>` + 自定义 deleter 管理资源回收，结合条件变量实现借还模型。  
  - 任何获取到的 `std::shared_ptr<RedisClient>` 都具备自动归还能力，方便在业务层安全使用。

- **`mq/` —— RabbitMQ 集成层**  
  - `MQHandler` 将 AMQP-CPP 的 `TcpHandler` 嵌入 `EventLoop`，以 `Channel` 监听底层 fd，可同时响应读写事件。  
  - `MQClient`、`MQProducer`、`MQConsumer` 把连接、发布、消费封装为高阶接口。  
  - `interface/mq/MQEventRouter` 负责事件注册与分发，支持动态扩展业务处理器，实现“MQ→领域服务”的桥梁。

- **`apps/order_server/` —— 业务演示层**  
  - **`app/`**：`OrderApplication` 汇聚配置解析（`OrderServerOptions::FromConfig`）、HTTP/MQ/DB/Redis 初始化、缓存预热、事件路由启动。  
  - **`domain/`**：抽象 `OrderService`、`InventoryService`，展示聚合根、库存预留、状态变迁等领域逻辑。  
  - **`infra/`**：按职责划分 `db/OrderRepository`、`cache/OrderCache`、`inventory/InventoryRepository`、`mq/OrderEventConsumer`，让业务逻辑与存储/外部系统隔离。  
  - **`interface/`**：`OrderCreateHandler`、`OrderQueryHandler`、`MQEventRouter` 将 HTTP/MQ 请求映射为领域操作，并通过依赖注入解耦。  
  - 这一层串联起启动、配置、持久化、缓存、消息与接口，真实呈现“从网络层到应用层”的端到端调用链。

- **周边配套**  
  - `tests/` 提供 echo/client 演示，验证核心网络栈。  
  - `docker-compose.yml` 编排 MySQL/Redis/RabbitMQ，助力本地快速落地。  
  - `test.sh` 串联健康检查、下单、DB 校验、Redis 命中，方便回归。

## 关键文件
- `core/src/EventLoop.cpp`：事件循环主实现，负责调度 IO 事件与任务队列。
- `core/include/EPollPoller.h`：封装 epoll 行为的 Poller 抽象与实现接口。
- `net/include/HttpServer.h`：HTTP 服务入口，关联路由、中间件、Session 与 TLS。
- `db/src/MySQLConnPool.cpp`：MySQL 连接池逻辑，处理池化、超时与重连策略。
- `cache/RedisPool.cpp`：Redis 连接池实现，演示智能指针与条件变量协作。
- `mq/include/MQHandler.h`：将 AMQP-CPP 与 `EventLoop` 对接的核心桥梁。
- `apps/order_server/app/OrderApplication.cpp`：应用装配与依赖初始化的总控。
- `apps/order_server/domain/OrderService.cpp`：订单领域逻辑与状态流转示例。
- `apps/order_server/interface/http/OrderCreateHandler.cpp`：下单接口完整流程。
- `apps/order_server/interface/mq/MQEventRouter.cpp`：MQ 事件路由与处理注册。

## 关键能力速览
- `EventLoopThreadPool` 负责 Reactor 线程扩展，确保多核场景下连接分配均衡。
- Redis、RabbitMQ 的 socket 事件完全由自研 `EventLoop` 驱动，实现跨中间件的统一调度模型。
- 业务层以 `Dependencies` 结构体显式声明依赖，让测试替换、Mock 与模块拆分更加顺滑。
- 日志与错误处理贯穿各层，辅以缓存预热、健康检查、优雅停机，强调生产可运维性。

## 简要使用提示
项目使用 CMake 构建，依赖 `mysqlcppconn`、`hiredis`、`yaml-cpp`、`nlohmann_json`、`amqpcpp` 与 `OpenSSL`。准备好依赖后在 `build/` 目录执行常规 CMake 构建即可产出 `bin/order_server`，运行时通过 `apps/order_server/config/config.yaml` 或命令行 `--config`/环境变量覆盖配置，实现端到端演示。

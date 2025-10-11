# muduo_plus

> 一套从 Reactor 内核到订单业务全链打通的现代 C++20 服务端样板工程

## 技术亮点
- 纯手写 Reactor：`core/` 模块内自实现 `EventLoop`、`Channel`、`EPollPoller`、`TcpServer`，线程安全的 `runInLoop`/`queueInLoop` 让多核扩展自然完成。
- 工程化 Logger：`logger/` 提供统一宏与 `std::source_location` 注入，伴随全局 `LogLevel`、滚动文件与彩色控制台输出。
- 中间件一体化：MySQL、Redis、RabbitMQ、YAML、JSON 均以 RAII + 智能指针封装；`RedisPool` 具备自动重连，`MQClient` 按生命周期关闭连接。
- 真实业务演示：`apps/order_server` 按 App/Domain/Infra/Interface 分层，涵盖库存预留、缓存回填、订单事件发布与消费。
- DevOps 配套：模块化 CMake、`docker-compose.yml`、一键巡检脚本 `test.sh`、可选 Prometheus 指标端口，便于课堂到生产的迁移。

## 分层结构总览
- **Core Layer**：基于 epoll 的 Reactor 内核，包含线程池、Buffer、Timestamp 与跨线程任务派发。
- **Network & Middleware Layer**：`net/` HTTP/TLS 协议栈搭配中间件适配器，RabbitMQ handler 直接挂入事件循环。
- **Data Layer**：`db/` 的 `MySQLConnPool`、`cache/` 的 `RedisPool`，以及 JSON/YAML 配置解析。
- **Application Layer**：订单域模型、仓储、HTTP/MQ 处理器、自定义依赖装配、缓存预热。
- **Operations Layer**：日志、配置、脚本、Docker、健康检查、巡检工具，构建完整的运维闭环。

## 目录导航
```
muduo_plus/
├── core/              # Reactor 与底层基元
├── net/               # HTTP/TLS、Session、Middleware
├── db/                # MySQL 连接池与仓储工具
├── cache/             # RedisClient / RedisPool
├── mq/                # AMQP-CPP 集成与 MQProducer/Consumer
├── apps/order_server/ # 订单服务（app/domain/infra/interface/config）
├── logger/            # 日志实现与宏定义
├── tests/             # 基础网络示例与单元测试
├── docker-compose.yml # MySQL + Redis + RabbitMQ 一键启动
└── test.sh            # 端到端巡检脚本
```

## 构建与运行

### 准备依赖
- CMake ≥ 3.15、C++20 编译器（GCC 11+/Clang 14+）
- 系统库：`pthread`、`mysqlcppconn`、`hiredis`、`yaml-cpp`、`nlohmann_json`、`amqpcpp`、`OpenSSL`
- 可选：Docker & Docker Compose（快速启动依赖服务）

### 快速启动
1. 启动依赖服务
   ```bash
   docker compose up -d mysql redis rabbitmq
   ```
2. 配置数据库
   ```bash
   mysql -h127.0.0.1 -uroot -e "CREATE DATABASE IF NOT EXISTS order_db;"
   ```
3. 构建项目
   ```bash
   mkdir -p build && cd build
   cmake ..
   cmake --build . -j
   ```
4. 运行订单服务
   ```bash
   export ORDER_SERVER_CONFIG=apps/order_server/config/config.yaml
   ./bin/order_server --config ${ORDER_SERVER_CONFIG}
   ```
5. 使用 `test.sh` 巡检
   ```bash
   ./test.sh
   ```

> 若本地已安装所需依赖，可跳过 Docker；`ORDER_SERVER_CONFIG` 环境变量优先级高于命令行 `--config`。

## 配置说明

`apps/order_server/config/config.yaml` 统一管理服务名、线程数、中间件、缓存、MQ、日志等配置，可通过环境变量或命令行覆盖。示例片段：

```yaml
serviceName: "order_server"
httpThreadNum: 4

database:
  connInfo:
    url: "tcp://127.0.0.1:3306"
    user: "root"
    database: "order_db"
    timeout_sec: 5
  maxConnections: 16

redis:
  host: "127.0.0.1"
  poolSize: 8
  timeoutMs: 1000

mq:
  url: "amqp://guest:guest@127.0.0.1:5672/"
  exchange: "order.exchange"
  enableConsumer: true
```

`OrderServerOptions::FromConfig` 借助 `ConfigLoader` 自动读取嵌套字段，并在初始化阶段校验必填字段，避免运行期故障。

## HTTP API

- `GET /health`：健康检查，返回 `{"status":"ok"}`。
- `POST /orders`：创建订单，需要 `userId`、`productId`、`quantity`、`amount`、`currency`。
- `GET /orders?id={orderId}`：按订单号查询，缓存命中优先。
- `GET /orders?userId={uid}&limit=20`：按用户分页查询，同时回填缓存。

示例请求：

```bash
curl -X POST http://127.0.0.1:8080/orders \
  -H "Content-Type: application/json" \
  -d '{"userId":"u1","productId":"sku-1","quantity":2,"amount":199.0,"currency":"CNY"}'
```

响应包含 `orderId`、订单状态以及金额、时间戳等信息。

## 消息与缓存联动
- `OrderCreateHandler`：下单后写库、更新 Redis、发布 MQ（`order.exchange` + routing key），失败自动释放库存。
- `OrderQueryHandler`：可选缓存优先策略，列表与详情查询分别维护索引、详情键并在 DB 命中后回填。
- `RedisPool`：基于 `std::queue<std::unique_ptr<RedisClient>>` 实现借还模型，支持超时、断线重连、条件变量唤醒。
- `MQEventRouter`：消费 RabbitMQ 事件，路由订单创建/支付/取消与库存释放，支持动态注册扩展。

## 日志与可观测性
- `LogMacros.h` 注入源文件、行号和线程 ID，`logger/` 支持控制台与文件双写，日志级别可在配置中调整。
- `OrderApplication` 启动期执行 Schema 校验、缓存预热、MQ/Redis/DB 初始化，日志逐步输出诊断信息。
- Prometheus 端口配置（默认 9090）保留占位，后续可扩展指标暴露。

## 巡检与调试
- `./test.sh` 会校验进程状态、HTTP 健康、下单、查询、数据库入库、Redis 缓存与最终摘要。
- `tests/` 目录提供 echo/client 示例，可在 `ctest` 或单独执行验证网络栈。
- 推荐在 Debug 构建模式下配合 `gdb` 与日志定位问题，`OrderApplication` 可启用更多 DEBUG 级日志输出。

欢迎在此基础上扩展更多协议、领域服务或观测指标，muduo_plus 旨在为现代 C++ 服务端开发提供开箱即用的教学与实战蓝本。

# syntax=docker/dockerfile:1

############################################################
# 构建阶段：安装依赖并编译 order_server
############################################################
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        pkg-config \
        libssl-dev \
        libev-dev \
        libhiredis-dev \
        libyaml-cpp-dev \
        libmysqlcppconn-dev \
        nlohmann-json3-dev \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# 编译安装 AMQP-CPP（开启 Linux TCP 支持）
RUN git clone --depth 1 https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git /tmp/amqp-cpp && \
    cmake -S /tmp/amqp-cpp -B /tmp/amqp-cpp/build \
        -DAMQP-CPP_LINUX_TCP=ON \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON && \
    cmake --build /tmp/amqp-cpp/build --target install -j"$(nproc)" && \
    rm -rf /tmp/amqp-cpp

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target order_server -j"$(nproc)"

############################################################
# 运行阶段：仅保留运行所需文件与依赖
############################################################
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libhiredis0.14 \
        libyaml-cpp0.7 \
        libmysqlcppconn8 \
        libssl3 \
        libev4 \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# 拷贝 AMQP-CPP 运行时库
COPY --from=builder /usr/local/lib/libamqpcpp.so* /usr/local/lib/
COPY --from=builder /usr/local/include/amqpcpp /usr/local/include/amqpcpp
RUN ldconfig

# 拷贝编译产物与默认配置
COPY --from=builder /app/bin/order_server /usr/local/bin/order_server
COPY --from=builder /app/apps/order_server/config /opt/order_server/config

WORKDIR /data

# 默认配置文件路径，可在运行时覆盖
ENV ORDER_SERVER_CONFIG=/opt/order_server/config/config.yaml

CMD ["/usr/local/bin/order_server"]

file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(mq SHARED ${SRC_FILES})

target_include_directories(mq PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/core/include
)

target_link_libraries(mq PUBLIC core amqpcpp pthread)
# # 1️⃣ 安装依赖
# sudo apt update
# sudo apt install -y git cmake build-essential libssl-dev libev-dev

# # 2️⃣ 重新克隆源码
# cd ~
# git clone https://github.com/CopernicaMarketingSoftware/AMQP-CPP.git
# cd AMQP-CPP

# # 3️⃣ 创建独立构建目录
# mkdir build && cd build

# # 4️⃣ 启用 Linux TCP 版本，并强制生成动态库和位置无关代码
# cmake .. \
#   -DAMQP-CPP_LINUX_TCP=ON \
#   -DBUILD_SHARED_LIBS=ON \
#   -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
#   -DCMAKE_BUILD_TYPE=Release

# # 5️⃣ 编译并安装
# make -j$(nproc)
# sudo make install

# # 6️⃣ 刷新动态库缓存
# sudo ldconfig /usr/local/lib

# # 7️⃣ 检查是否安装成功
# ls -l /usr/local/lib | grep amqpcpp

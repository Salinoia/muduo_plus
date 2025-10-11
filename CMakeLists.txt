# 设置最低版本和项目名称
cmake_minimum_required(VERSION 3.15)
project(muduo-core LANGUAGES CXX)

# 设置全局 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用测试
enable_testing()

# 输出路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(LIBS pthread)

# ----------------------------
# 编译选项：全局警告控制
# ----------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wno-unused-parameter
        -Wno-sign-conversion
        -Wnon-virtual-dtor
        -Wno-sign-conversion       # 允许有符号/无符号混合
        -Wno-implicit-float-conversion  # 放宽浮点精度警告
        -Werror
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
        /W4 /permissive- /Zc:__cplusplus /EHsc /utf-8
        /D_CRT_SECURE_NO_WARNINGS
    )
endif()

# ----------------------------
# 默认构建类型（Debug 优先）
# ----------------------------
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose Release or Debug" FORCE)
endif()

# ----------------------------
# 添加子目录
# ----------------------------
add_subdirectory(apps/order_server)
add_subdirectory(base)
add_subdirectory(cache)
add_subdirectory(core)
add_subdirectory(db)
add_subdirectory(logger)
add_subdirectory(mq)
add_subdirectory(net)
add_subdirectory(tests)

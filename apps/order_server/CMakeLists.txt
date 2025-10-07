file(GLOB_RECURSE ORDER_SERVER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/domain/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infra/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/*.cpp
)

# 生成可执行文件
add_executable(order_server ${ORDER_SERVER_SRC})

# 头文件搜索路径
target_include_directories(order_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/infra
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
)

# 链接依赖库
target_link_libraries(order_server PRIVATE
    core
    net
    db
    cache
    mq
    yaml-cpp
    pthread
)

# 输出路径
set_target_properties(order_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

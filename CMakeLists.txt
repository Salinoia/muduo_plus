#设置最低版本和项目名称
cmake_minimum_required(VERSION 3.15)
project(muduo-core)

#设置全局的 C++ 标准
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

enable_testing()

#链接必要的库
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(LIBS pthread)

#添加子目录
add_subdirectory(apps/order_server)
add_subdirectory(cache)
add_subdirectory(core)
add_subdirectory(db)
add_subdirectory(mq)
add_subdirectory(net)
add_subdirectory(tests)

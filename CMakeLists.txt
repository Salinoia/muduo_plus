#设置最低版本和项目名称
cmake_minimum_required(VERSION 3.10)
project(muduo-core)

#设置全局的c++标准
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED True)

enable_testing()

#链接必要的库
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(LIBS pthread)

#添加子目录
add_subdirectory(muduo_core)
add_subdirectory(http_server)


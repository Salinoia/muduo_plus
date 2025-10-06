file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(mq SHARED ${SRC_FILES})

target_include_directories(mq PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/muduo_core/include
)

target_link_libraries(mq PUBLIC muduo_core amqpcpp pthread)

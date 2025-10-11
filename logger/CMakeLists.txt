file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(logger SHARED ${SRC_FILES})

target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(logger PUBLIC base Threads::Threads)

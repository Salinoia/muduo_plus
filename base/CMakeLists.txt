file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(base SHARED ${SRC_FILES})

target_include_directories(base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Threads REQUIRED)
target_link_libraries(base PUBLIC Threads::Threads)

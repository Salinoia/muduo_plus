file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_library(cache SHARED ${SRC_FILES})

target_include_directories(cache PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cache PRIVATE hiredis pthread)

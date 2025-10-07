file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(net SHARED ${SRC_FILES})
target_include_directories(net PUBLIC ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/net/include)
target_link_libraries(net PUBLIC core)


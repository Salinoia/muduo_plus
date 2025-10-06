add_executable(echo EchoServer.cpp)
target_link_libraries(echo muduo_core ${LIBS})

add_executable(client Client.cpp)
target_link_libraries(client muduo_core ${LIBS})

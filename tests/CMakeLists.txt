add_executable(echo EchoServer.cpp)
target_link_libraries(echo core ${LIBS})

add_executable(client Client.cpp)
target_link_libraries(client core logger ${LIBS})

set_target_properties(echo client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

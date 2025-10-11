file(GLOB SRC src/*.cpp)
add_library(db SHARED ${SRC})

target_include_directories(db PUBLIC include)

find_library(MYSQLCPP NAMES mysqlcppconn8 mysqlcppconn)
find_package(nlohmann_json REQUIRED)
find_package(Threads)
target_link_libraries(db PRIVATE ${MYSQLCPP} core Threads::Threads nlohmann_json::nlohmann_json)

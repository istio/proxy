add_executable(http-server-example server.cpp)

target_include_directories(http-server-example PRIVATE ../common)

target_link_libraries(http-server-example dd-trace-cpp::static)

install(TARGETS http-server-example)

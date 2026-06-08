add_executable(baggage-example main.cpp)
target_link_libraries(baggage-example dd-trace-cpp::static)

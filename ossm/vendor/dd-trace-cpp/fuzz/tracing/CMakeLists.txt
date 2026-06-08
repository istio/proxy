add_executable(baggage-fuzz baggage.cpp)

add_dependencies(baggage-fuzz dd_trace_cpp-static)

target_include_directories(baggage-fuzz
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(baggage-fuzz dd_trace_cpp-static)

add_target_to_group(baggage-fuzz dd_trace_cpp-fuzzers)

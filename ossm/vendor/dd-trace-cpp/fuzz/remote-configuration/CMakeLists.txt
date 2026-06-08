add_executable(remote-config-fuzz main.cpp)

add_dependencies(remote-config-fuzz dd_trace_cpp-static)

target_include_directories(remote-config-fuzz
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/datadog
)

target_link_libraries(remote-config-fuzz dd_trace_cpp-static)

add_target_to_group(remote-config-fuzz dd_trace_cpp-fuzzers)

add_executable(w3c-propagation-fuzz fuzz.cpp)

add_dependencies(w3c-propagation-fuzz dd_trace_cpp-static)

target_include_directories(w3c-propagation-fuzz
  PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(w3c-propagation-fuzz dd_trace_cpp-static)

add_target_to_group(w3c-propagation-fuzz dd_trace_cpp-fuzzers)

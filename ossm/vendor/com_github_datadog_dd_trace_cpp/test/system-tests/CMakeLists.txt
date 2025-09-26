add_executable(parametric-http-server)

target_sources(parametric-http-server
  PRIVATE
  main.cpp
  developer_noise.cpp
  request_handler.cpp
)

target_include_directories(parametric-http-server
  PRIVATE
  ../../examples/http-server/common
)

target_link_libraries(parametric-http-server dd_trace_cpp-static dd_trace::specs)

install(
  TARGETS parametric-http-server
  DESTINATION bin
)

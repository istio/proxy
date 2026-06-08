add_executable(parametric-http-server)

target_sources(parametric-http-server
  PRIVATE
  main.cpp
  developer_noise.cpp
  request_handler.cpp
)

target_include_directories(parametric-http-server
  PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/examples/http-server/common
)

target_link_libraries(parametric-http-server 
  PRIVATE
    dd-trace-cpp::static 
    dd-trace-cpp::specs
    nlohmann_json::nlohmann_json
)

install(
  TARGETS parametric-http-server
  DESTINATION bin
)

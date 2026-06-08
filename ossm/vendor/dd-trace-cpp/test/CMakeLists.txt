add_executable(tests
    # test driver
    main.cpp

    # wrapper around Catch2
    test.cpp

    # mocks
    mocks/collectors.cpp
    mocks/dict_readers.cpp
    mocks/dict_writers.cpp
    mocks/event_schedulers.cpp
    mocks/http_clients.cpp
    mocks/loggers.cpp

    # utilities
    matchers.cpp

    # telemetry test cases
    telemetry/test_configuration.cpp
    telemetry/test_telemetry.cpp

    # test cases
    test_baggage.cpp
    test_base64.cpp
    test_cerr_logger.cpp
    test_config_manager.cpp
    test_datadog_agent.cpp
    test_glob.cpp
    test_limiter.cpp
    test_msgpack.cpp
    test_platform_util.cpp
    test_parse_util.cpp
    test_smoke.cpp
    test_span.cpp
    test_span_sampler.cpp
    test_trace_id.cpp
    test_trace_segment.cpp
    test_tracer_config.cpp
    test_tracer.cpp
    test_trace_sampler.cpp

    remote_config/test_remote_config.cpp
)

target_include_directories(tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/datadog
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/datadog
)

# Enable Catch2 std stringification 
# <https://github.com/catchorg/Catch2/blob/devel/docs/configuration.md#enabling-stringification>
target_compile_definitions(tests
  PUBLIC
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
)

target_link_libraries(tests 
  PRIVATE
    dd-trace-cpp::static
    dd-trace-cpp::specs
)

if(DD_TRACE_TRANSPORT STREQUAL "curl")
  target_sources(tests PRIVATE test_curl.cpp)
  target_link_libraries(tests 
    PRIVATE
      # TODO: Remove dependency on libcurl
      CURL::libcurl_static
  )
endif()

catch_discover_tests(tests)


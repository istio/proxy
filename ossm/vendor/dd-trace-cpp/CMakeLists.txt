# Note: Make sure that this version is the same as that in
# "./CheckRequiredCMakeVersion.cmake".
cmake_minimum_required(VERSION 3.28...4.0)
cmake_policy(SET CMP0077 NEW)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Get the version of the lib
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/datadog/version.cpp DD_TRACE_VERSION_CPP_CONTENTS REGEX "#define DD_TRACE_VERSION( |_NUM )")
string(REGEX MATCH "#define DD_TRACE_VERSION \"[^\"]*" DD_TRACE_VERSION ${DD_TRACE_VERSION_CPP_CONTENTS})
string(REGEX REPLACE "[^\"]+\"v" "" DD_TRACE_VERSION ${DD_TRACE_VERSION})

project(
  dd-trace-cpp
    VERSION ${DD_TRACE_VERSION}
    LANGUAGES CXX
)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_STATIC_LIBS "Build static libraries" ON)

if (WIN32)
  option(DD_TRACE_STATIC_CRT "Build dd-trace-cpp with static CRT with MSVC" OFF)
endif ()

if (DD_TRACE_STATIC_CRT)
  set(CURL_STATIC_CRT ON)
endif ()

set(DD_TRACE_TRANSPORT "curl" CACHE STRING "HTTP transport that dd-trace-cpp uses to communicate with the Datadog Agent, can be either 'none' or 'curl'")

if(DD_TRACE_TRANSPORT STREQUAL "curl")
  include(cmake/deps/curl.cmake)
elseif(DD_TRACE_TRANSPORT STREQUAL "none")
    message(STATUS "DD_TRACE_TRANSPORT is set to 'none', no default transport will be included")
else()
  message(FATAL_ERROR "Invalid value for DD_TRACE_TRANSPORT: ${DD_TRACE_TRANSPORT}")
endif()

# Consumer of the library using FetchContent do not need
# to build unit tests, fuzzers and examples.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  option(DD_TRACE_BUILD_EXAMPLES "Build example programs" OFF)
  option(DD_TRACE_BUILD_TESTING "Build the unit tests (test/)" OFF)
  option(DD_TRACE_BUILD_FUZZERS "Build fuzzers" OFF)
  option(DD_TRACE_BUILD_BENCHMARK "Build benchmark binaries" OFF)
  option(DD_TRACE_ENABLE_COVERAGE "Build code with code coverage profiling instrumentation" OFF)
  option(DD_TRACE_ENABLE_SANITIZE "Build with address sanitizer and undefined behavior sanitizer" OFF)
endif()

# Include mandatory files
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Linking this library requires threads.
find_package(Threads REQUIRED)
include(cmake/deps/json.cmake)
include(cmake/utils.cmake)

message(STATUS "dd-trace-cpp version=[${DD_TRACE_VERSION}]")
unset(DD_TRACE_VERSION_CPP_CONTENTS)

message(STATUS "dd-trace-cpp transport=${DD_TRACE_TRANSPORT}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT MATCHES "MSVC"))
  if (BUILD_SHARED_LIBS AND BUILD_STATIC_LIBS)
    message(FATAL_ERROR "Can't build both static and shared libary for MSVC")
  endif ()
  message(STATUS "Compiler: MSVC ${CMAKE_CXX_COMPILER_VERSION}")
  include(cmake/compiler/msvc.cmake)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(STATUS "Compiler: clang ${CMAKE_CXX_COMPILER_VERSION}")
  include(cmake/compiler/clang.cmake)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  message(STATUS "Compiler: clang-apple ${CMAKE_CXX_COMPILER_VERSION}")
  include(cmake/compiler/clang_apple.cmake)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(STATUS "Compiler: GCC ${CMAKE_CXX_COMPILER_VERSION}")
  include(cmake/compiler/gcc.cmake)
endif ()

if (DD_TRACE_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif ()

if (DD_TRACE_BUILD_TESTING)
  include(Catch)
  enable_testing()
  add_subdirectory(test)
  add_subdirectory(test/system-tests)
endif()

if (DD_TRACE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif ()

if (DD_TRACE_BUILD_BENCHMARK)
  add_subdirectory(benchmark)
endif ()

add_library(dd-trace-cpp-objects OBJECT)
add_library(dd-trace-cpp::obj ALIAS dd-trace-cpp-objects)

target_compile_features(dd-trace-cpp-objects
  PUBLIC
    cxx_std_17
)

# NOTE: Don't use glob:
#  - <https://stackoverflow.com/questions/32411963/why-is-cmake-file-glob-evil>
#  - <https://discourse.cmake.org/t/is-glob-still-considered-harmful-with-configure-depends/808>
target_sources(dd-trace-cpp-objects
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS include
    FILES 
      include/datadog/remote_config/capability.h
      include/datadog/remote_config/listener.h
      include/datadog/remote_config/product.h
      include/datadog/telemetry/configuration.h
      include/datadog/telemetry/metrics.h
      include/datadog/telemetry/product.h
      include/datadog/telemetry/telemetry.h
      include/datadog/baggage.h
      include/datadog/cerr_logger.h
      include/datadog/clock.h
      include/datadog/collector.h
      include/datadog/config.h
      include/datadog/datadog_agent_config.h
      include/datadog/dict_reader.h
      include/datadog/dict_writer.h
      include/datadog/environment.h
      include/datadog/error.h
      include/datadog/event_scheduler.h
      include/datadog/expected.h
      include/datadog/http_client.h
      include/datadog/id_generator.h
      include/datadog/injection_options.h
      include/datadog/logger.h
      include/datadog/null_collector.h
      include/datadog/optional.h
      include/datadog/propagation_style.h
      include/datadog/rate.h
      include/datadog/runtime_id.h
      include/datadog/sampling_decision.h
      include/datadog/sampling_mechanism.h
      include/datadog/sampling_priority.h
      include/datadog/span.h
      include/datadog/span_config.h
      include/datadog/span_defaults.h
      include/datadog/span_matcher.h
      include/datadog/span_sampler_config.h
      include/datadog/string_view.h
      include/datadog/trace_id.h
      include/datadog/trace_sampler_config.h
      include/datadog/trace_segment.h
      include/datadog/trace_source.h
      include/datadog/tracer.h
      include/datadog/tracer_config.h
      include/datadog/tracer_signature.h
      include/datadog/version.h
  PRIVATE
    src/datadog/common/hash.cpp
    src/datadog/telemetry/configuration.cpp
    src/datadog/telemetry/telemetry.cpp
    src/datadog/telemetry/telemetry_impl.cpp
    src/datadog/baggage.cpp
    src/datadog/base64.cpp
    src/datadog/cerr_logger.cpp
    src/datadog/clock.cpp
    src/datadog/config_manager.cpp
    src/datadog/collector_response.cpp
    src/datadog/datadog_agent_config.cpp
    src/datadog/datadog_agent.cpp
    src/datadog/environment.cpp
    src/datadog/error.cpp
    src/datadog/extraction_util.cpp
    src/datadog/glob.cpp
    src/datadog/http_client.cpp
    src/datadog/id_generator.cpp
    src/datadog/limiter.cpp
    src/datadog/logger.cpp
    src/datadog/msgpack.cpp
    src/datadog/parse_util.cpp
    src/datadog/platform_util.cpp
    src/datadog/propagation_style.cpp
    src/datadog/random.cpp
    src/datadog/rate.cpp
    src/datadog/remote_config/product.cpp
    src/datadog/remote_config/remote_config.cpp
    src/datadog/runtime_id.cpp
    src/datadog/span.cpp
    src/datadog/span_data.cpp
    src/datadog/span_matcher.cpp
    src/datadog/span_sampler_config.cpp
    src/datadog/span_sampler.cpp
    src/datadog/string_util.cpp
    src/datadog/tags.cpp
    src/datadog/tag_propagation.cpp
    src/datadog/threaded_event_scheduler.cpp
    src/datadog/tracer_config.cpp
    src/datadog/tracer.cpp
    src/datadog/trace_id.cpp
    src/datadog/trace_sampler_config.cpp
    src/datadog/trace_sampler.cpp
    src/datadog/trace_segment.cpp
    src/datadog/trace_source.cpp
    src/datadog/telemetry_metrics.cpp
    src/datadog/version.cpp
    src/datadog/w3c_propagation.cpp
)

target_include_directories(dd-trace-cpp-objects
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datadog
)

target_link_libraries(dd-trace-cpp-objects
  PUBLIC
    Threads::Threads
  PRIVATE
    dd-trace-cpp::specs
)

set_target_properties(dd-trace-cpp-objects
  PROPERTIES
    VERIFY_INTERFACE_HEADER_SETS ON
    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

install(
  TARGETS dd-trace-cpp-objects dd-trace-cpp-specs
  EXPORT dd-trace-cpp-targets
  FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Produce both shared and static versions of the library.
if (BUILD_SHARED_LIBS)
  add_library(dd-trace-cpp-shared SHARED $<TARGET_OBJECTS:dd-trace-cpp-objects>)
  add_library(dd-trace-cpp::shared ALIAS dd-trace-cpp-shared)

  if(DD_TRACE_TRANSPORT STREQUAL "curl")
    add_dependencies(dd-trace-cpp-shared CURL::libcurl_shared)

    target_sources(dd-trace-cpp-shared
      PRIVATE
        src/datadog/curl.cpp
        src/datadog/default_http_client_curl.cpp
    )

    target_link_libraries(dd-trace-cpp-shared
      PRIVATE
        CURL::libcurl_shared
    )

    install(
      TARGETS libcurl_shared
      EXPORT dd-trace-cpp-targets
    )
  else()
    target_sources(dd-trace-cpp-shared
      PRIVATE
        src/datadog/default_http_client_null.cpp
    )
  endif()

  target_link_libraries(dd-trace-cpp-shared
    PUBLIC
      dd-trace-cpp::obj
    PRIVATE
      dd-trace-cpp::specs
  )

  set_target_properties(dd-trace-cpp-shared
    PROPERTIES 
      OUTPUT_NAME "dd-trace-cpp"
  )

  install(
    TARGETS dd-trace-cpp-shared 
    EXPORT dd-trace-cpp-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif ()

if (BUILD_STATIC_LIBS)
  add_library(dd-trace-cpp-static STATIC $<TARGET_OBJECTS:dd-trace-cpp-objects>)
  add_library(dd-trace-cpp::static ALIAS dd-trace-cpp-static)

  add_dependencies(dd-trace-cpp-static dd-trace-cpp-objects)

  if (DD_TRACE_TRANSPORT STREQUAL "curl")
    add_dependencies(dd-trace-cpp-static CURL::libcurl_static)

    target_sources(dd-trace-cpp-static
      PRIVATE
        src/datadog/curl.cpp
        src/datadog/default_http_client_curl.cpp
    )

    target_link_libraries(dd-trace-cpp-static 
      PRIVATE
        CURL::libcurl_static
    )

    install(
      TARGETS libcurl_static
      EXPORT dd-trace-cpp-targets
    )
  else()
    target_sources(dd-trace-cpp-static
      PRIVATE
        src/datadog/default_http_client_null.cpp
    ) 
  endif ()

  target_link_libraries(dd-trace-cpp-static 
    PUBLIC
      dd-trace-cpp::obj
    PRIVATE
      dd-trace-cpp::specs
  )

  set_target_properties(dd-trace-cpp-static 
    PROPERTIES 
      OUTPUT_NAME "dd-trace-cpp"
  )

  install(
    TARGETS dd-trace-cpp-static
    EXPORT dd-trace-cpp-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif ()

# install target
write_basic_package_version_file(
  "${PROJECT_NAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in"
  ${PROJECT_NAME}-config.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
  PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

install(
  EXPORT dd-trace-cpp-targets
  NAMESPACE dd-trace-cpp::
  FILE ${PROJECT_NAME}-targets.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
  FILES 
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

# Note: Make sure that this version is the same as that in
# "./CheckRequiredCMakeVersion.cmake".
cmake_minimum_required(VERSION 3.24)

project(dd-trace-cpp)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_STATIC_LIBS "Build static libraries" OFF)

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

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(DD_TRACE_ENABLE_SANITIZE ON)
endif ()

# Linking this library requires libcurl and threads.
find_package(Threads REQUIRED)
include(cmake/deps/curl.cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/datadog/version.cpp DD_TRACE_VERSION_CPP_CONTENTS REGEX "#define DD_TRACE_VERSION( |_NUM )")
string(REGEX MATCH "#define DD_TRACE_VERSION \"[^\"]*" DD_TRACE_VERSION ${DD_TRACE_VERSION_CPP_CONTENTS})
string(REGEX REPLACE "[^\"]+\"" "" DD_TRACE_VERSION ${DD_TRACE_VERSION})
message(STATUS "dd-trace-cpp version=[${DD_TRACE_VERSION}]")
unset(DD_TRACE_VERSION_CPP_CONTENTS)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
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
  add_subdirectory(test)
endif()

if (DD_TRACE_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif ()

if (DD_TRACE_BUILD_BENCHMARK)
  add_subdirectory(benchmark)
endif ()

add_library(dd_trace_cpp-objects OBJECT)
add_library(dd_trace::obj ALIAS dd_trace_cpp-objects)

add_dependencies(dd_trace_cpp-objects libcurl)

target_sources(dd_trace_cpp-objects
  PRIVATE
    src/datadog/base64.cpp
    src/datadog/cerr_logger.cpp
    src/datadog/clock.cpp
    src/datadog/config_manager.cpp
    src/datadog/collector_response.cpp
    src/datadog/curl.cpp
    src/datadog/datadog_agent_config.cpp
    src/datadog/datadog_agent.cpp
    src/datadog/default_http_client_curl.cpp
#     src/datadog/default_http_client_null.cpp use libcurl
    src/datadog/environment.cpp
    src/datadog/error.cpp
    src/datadog/extraction_util.cpp
    src/datadog/glob.cpp
    src/datadog/http_client.cpp
    src/datadog/id_generator.cpp
    src/datadog/limiter.cpp
    src/datadog/logger.cpp
    src/datadog/metrics.cpp
    src/datadog/msgpack.cpp
    src/datadog/null_collector.cpp
    src/datadog/parse_util.cpp
    src/datadog/platform_util.cpp
    src/datadog/propagation_style.cpp
    src/datadog/random.cpp
    src/datadog/rate.cpp
    src/datadog/remote_config.cpp
    src/datadog/runtime_id.cpp
    src/datadog/span.cpp
    src/datadog/span_data.cpp
    src/datadog/span_defaults.cpp
    src/datadog/span_matcher.cpp
    src/datadog/span_sampler_config.cpp
    src/datadog/span_sampler.cpp
    src/datadog/string_util.cpp
    src/datadog/tags.cpp
    src/datadog/tag_propagation.cpp
    src/datadog/threaded_event_scheduler.cpp
    src/datadog/tracer_config.cpp
    src/datadog/tracer_telemetry.cpp
    src/datadog/tracer.cpp
    src/datadog/trace_id.cpp
    src/datadog/trace_sampler_config.cpp
    src/datadog/trace_sampler.cpp
    src/datadog/trace_segment.cpp
    src/datadog/version.cpp
    src/datadog/w3c_propagation.cpp
)

# This library's public headers are just its source headers.
target_sources(dd_trace_cpp-objects PUBLIC
  FILE_SET public_headers
  TYPE HEADERS
  BASE_DIRS src/
  FILES
  src/datadog/base64.h
  src/datadog/config.h
  src/datadog/cerr_logger.h
  src/datadog/clock.h
  src/datadog/config_manager.h
  src/datadog/config_update.h
  src/datadog/collector.h
  src/datadog/collector_response.h
  # src/datadog/curl.h except for curl.h
  src/datadog/datadog_agent_config.h
  src/datadog/datadog_agent.h
  src/datadog/default_http_client.h
  src/datadog/dict_reader.h
  src/datadog/dict_writer.h
  src/datadog/environment.h
  src/datadog/error.h
  src/datadog/event_scheduler.h
  src/datadog/expected.h
  src/datadog/extracted_data.h
  src/datadog/extraction_util.h
  src/datadog/glob.h
  src/datadog/hex.h
  src/datadog/http_client.h
  src/datadog/id_generator.h
  src/datadog/injection_options.h
  src/datadog/json_fwd.hpp
  src/datadog/json.hpp
  src/datadog/limiter.h
  src/datadog/logger.h
  src/datadog/metrics.h
  src/datadog/msgpack.h
  src/datadog/null_collector.h
  src/datadog/optional.h
  src/datadog/parse_util.h
  src/datadog/platform_util.h
  src/datadog/propagation_style.h
  src/datadog/random.h
  src/datadog/rate.h
  src/datadog/remote_config.h
  src/datadog/runtime_id.h
  src/datadog/sampling_decision.h
  src/datadog/sampling_mechanism.h
  src/datadog/sampling_priority.h
  src/datadog/sampling_util.h
  src/datadog/span_config.h
  src/datadog/span_data.h
  src/datadog/span_defaults.h
  src/datadog/span.h
  src/datadog/span_matcher.h
  src/datadog/span_sampler_config.h
  src/datadog/span_sampler.h
  src/datadog/string_util.h
  src/datadog/string_view.h
  src/datadog/tag_propagation.h
  src/datadog/tags.h
  src/datadog/threaded_event_scheduler.h
  src/datadog/tracer_config.h
  src/datadog/tracer_signature.h
  src/datadog/tracer_telemetry.h
  src/datadog/tracer.h
  src/datadog/trace_id.h
  src/datadog/trace_sampler_config.h
  src/datadog/trace_sampler.h
  src/datadog/trace_segment.h
  src/datadog/version.h
  src/datadog/w3c_propagation.h
)

# TODO: define public/private
# Headers location are different depending of whether we are building 
# or installing the library.
# target_include_directories(dd_trace_cpp-objects
#   PUBLIC
#     $<INSTALL_INTERFACE:src>
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
# )

target_link_libraries(dd_trace_cpp-objects
  PRIVATE
    libcurl
    Threads::Threads
    dd_trace::specs
)

# Produce both shared and static versions of the library.
if (BUILD_SHARED_LIBS)
  add_library(dd_trace_cpp-shared SHARED $<TARGET_OBJECTS:dd_trace_cpp-objects>)
  add_library(dd_trace::shared ALIAS dd_trace_cpp-shared)

  add_dependencies(dd_trace_cpp-shared dd_trace_cpp-objects)

  target_link_libraries(dd_trace_cpp-shared
    PUBLIC
      dd_trace::obj
    PRIVATE
      dd_trace::specs
  )

  install(TARGETS dd_trace_cpp-objects dd_trace_cpp-shared
    EXPORT dd_trace_cpp-export
    FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    # For Windows
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif ()

if (BUILD_STATIC_LIBS)
  add_library(dd_trace_cpp-static STATIC $<TARGET_OBJECTS:dd_trace_cpp-objects>)
  add_library(dd_trace::static ALIAS dd_trace_cpp-static)

  add_dependencies(dd_trace_cpp-static dd_trace_cpp-objects)

  target_link_libraries(dd_trace_cpp-static 
    PUBLIC
      dd_trace::obj
    PRIVATE
      dd_trace::specs
  )

  install(TARGETS dd_trace_cpp-objects dd_trace_cpp-static
    EXPORT dd_trace_cpp-export
    FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    # For Windows
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
endif ()

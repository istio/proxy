# Android uses 3.6, clion 3.7.1, trusty 2.8.12
cmake_minimum_required(VERSION 2.8)

project(istio)

set(ISTIO_BASE "" CACHE string "Istio base directory, default to current dir")
set(ISTIO_PROXY "." CACHE string "Proxy source directory")
set(ISTIO_NATIVE "vendor/src" CACHE string "Native source directory")
set(ISTIO_CMAKE "tools/cmake" CACHE string "Custom CMake files" FORCE)
# Duplicate gtest with boring ssl
set(YAML_CPP_BUILD_TESTS OFF CACHE bool "disable yaml gtest dup" FORCE)
set(ABSL_NO_TEST TRUE)

set(ISTIO_GENFILES "vendor/genfiles/bazel" CACHE string "Location of generated files" FORCE)
# TODO: convince envoy to preserve genfiles.
set(ISTIO_DEP_GENFILES "vendor/genfiles" CACHE string "Location of dependency genfiles. Envoy doesn't preserve them.")

# No effect on trusty - must set cxx flag
set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14" )

# Envoy uses its own boring ssl crypto, no need for it in libevent
set(EVENT__DISABLE_OPENSSL ON CACHE bool "Disable ssl")
set(EVENT__DISABLE_TESTS ON CACHE bool "disable tests" FORCE)
set(EVENT__DISABLE_SAMPLES ON CACHE bool "disable samples")
set(EVENT__DISABLE_BENCHMARK ON CACHE bool "disable bench")
set(YAML_CPP_BUILD_TOOLS OFF CACHE bool "disable tools" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE bool "dsable tests" FORCE)
set(ZLIB_LIBRARY "" CACHE string "no zlib" FORCE)

# To compile boring ssl - requires go in path
set(GO_EXECUTABLE "go")

set(BUILD_TESTING OFF CACHE bool "" FORCE)
# protobuf requires it
add_definitions(-DHAVE_PTHREAD)

set_property(GLOBAL PROPERTY OPENSSL_ROOT_DIR ${ISTIO_NATIVE}/boringssl/src)

# Projects already using cmake
# Doesn't work with CMake 2.8 on trusty
add_subdirectory(${ISTIO_NATIVE}/libevent)
add_subdirectory(${ISTIO_NATIVE}/boringssl/src EXCLUDE_FROM_ALL)
add_subdirectory(${ISTIO_NATIVE}/yaml-cpp)
add_subdirectory(${ISTIO_NATIVE}/xxhash/cmake_unofficial)
add_subdirectory(${ISTIO_NATIVE}/protobuf/cmake)
add_subdirectory(${ISTIO_NATIVE}/cctz)
#add_subdirectory(${ISTIO_NATIVE}/googletest)
#add_subdirectory(${ISTIO_NATIVE}/abseil-cpp)

# Packages without cmake support - custom files, similar with the custom bazel
include(${ISTIO_CMAKE}/cares.cmake)
include(${ISTIO_CMAKE}/lightstep-tracer.cmake)
include(${ISTIO_CMAKE}/http-parser.cmake)
include(${ISTIO_CMAKE}/nghttp2.cmake)
include(${ISTIO_CMAKE}/istio-mixer.cmake)
include(${ISTIO_CMAKE}/istio-proxy.cmake)
include(${ISTIO_CMAKE}/grpc_transcoding.cmake)

include(${ISTIO_CMAKE}/envoy.cmake)

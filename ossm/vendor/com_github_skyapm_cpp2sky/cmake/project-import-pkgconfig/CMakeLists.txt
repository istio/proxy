cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW) # recognize CMAKE_MSVC_RUNTIME_LIBRARY
endif()

project(cpp2sky-import-pkgconfig)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CPP2SKY_PKG REQUIRED cpp2sky)

find_package(fmt)
find_package(spdlog)
find_package(httplib)

find_package(Protobuf CONFIG REQUIRED)
set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(_REFLECTION gRPC::grpc++_reflection)
find_package(gRPC CONFIG REQUIRED)
set(_GRPC_GRPCPP gRPC::grpc++)


add_executable(example-sample sample.cc)
target_link_libraries(example-sample PRIVATE
    ${CPP2SKY_PKG_LIBRARIES}
    proto_lib
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    fmt::fmt
    spdlog::spdlog
    httplib::httplib
)

add_executable(example-sample-client sample_client.cc)
target_link_libraries(example-sample-client PUBLIC
    ${CPP2SKY_PKG_LIBRARIES}
    proto_lib
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    fmt::fmt
    spdlog::spdlog
    httplib::httplib
)

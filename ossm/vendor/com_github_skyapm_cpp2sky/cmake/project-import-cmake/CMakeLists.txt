cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW) # recognize CMAKE_MSVC_RUNTIME_LIBRARY
endif()

project(cpp2sky-import-cmake)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(cpp2sky CONFIG REQUIRED)

add_executable(example-sample sample.cc)
target_link_libraries(example-sample PUBLIC
    cpp2sky::cpp2sky
    proto_lib
)

add_executable(example-sample-client sample_client.cc)
target_link_libraries(example-sample-client PUBLIC
    cpp2sky::cpp2sky
    proto_lib
)

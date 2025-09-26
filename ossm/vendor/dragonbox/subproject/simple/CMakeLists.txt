cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(dragonbox_simple LANGUAGES CXX)

set(dragonbox_simple_headers include/simple_dragonbox.h)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19.0")
	add_library(dragonbox_simple INTERFACE ${dragonbox_simple_headers})
else()
	add_library(dragonbox_simple INTERFACE)
endif()
add_library(dragonbox::simple ALIAS dragonbox_simple)

target_include_directories(dragonbox_simple
        INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)

target_compile_features(dragonbox_simple INTERFACE cxx_std_17)
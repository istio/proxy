cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(ryu_cmake LANGUAGES C)

add_library(ryu STATIC
        ryu/ryu.h ryu/common.h ryu/f2s.c ryu/d2s.c
        ryu/f2s_intrinsics.h ryu/f2s_full_table.h
        ryu/d2s_intrinsics.h ryu/d2s_full_table.h
        ryu/digit_table.h)
add_library(ryu::ryu ALIAS ryu)

target_compile_features(ryu PUBLIC cxx_std_17)

target_include_directories(ryu
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)

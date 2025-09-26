cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(grisu_exact LANGUAGES CXX)

add_library(grisu_exact STATIC fp_to_chars.h fp_to_chars.cpp grisu_exact.h)
add_library(grisu_exact::grisu_exact ALIAS grisu_exact)

target_compile_features(grisu_exact PUBLIC cxx_std_17)

target_include_directories(grisu_exact
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
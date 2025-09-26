cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(dragonbox_common LANGUAGES CXX)

include(FetchContent)
if (NOT TARGET dragonbox)
    FetchContent_Declare(dragonbox SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
    FetchContent_MakeAvailable(dragonbox)
endif()

set(dragonbox_common_headers
        include/big_uint.h
	include/continued_fractions.h
	include/rational_continued_fractions.h
	include/best_rational_approx.h
	include/good_rational_approx.h
        include/random_float.h)

set(dragonbox_common_sources source/big_uint.cpp)

add_library(dragonbox_common STATIC
        ${dragonbox_common_headers}
        ${dragonbox_common_sources})
add_library(dragonbox::common ALIAS dragonbox_common)

target_include_directories(dragonbox_common
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)

target_compile_features(dragonbox_common PUBLIC cxx_std_17)

target_link_libraries(dragonbox_common PUBLIC dragonbox::dragonbox)
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(benchmark LANGUAGES CXX)

include(FetchContent)
if (NOT TARGET dragonbox)
    FetchContent_Declare(dragonbox SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
    FetchContent_MakeAvailable(dragonbox)
endif()
if (NOT TARGET common)
    FetchContent_Declare(common SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../common")
    FetchContent_MakeAvailable(common)
endif()
if (NOT TARGET ryu)
    FetchContent_Declare(ryu SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/ryu")
    FetchContent_MakeAvailable(ryu)
endif()
if (NOT TARGET schubfach)
    FetchContent_Declare(schubfach SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/schubfach")
    FetchContent_MakeAvailable(schubfach)
endif()
if (NOT TARGET grisu_exact)
    FetchContent_Declare(grisu_exact SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/grisu_exact")
    FetchContent_MakeAvailable(grisu_exact)
endif()

set(benchmark_headers include/benchmark.h)

set(benchmark_sources
        source/dragonbox.cpp
        source/grisu_exact.cpp
        source/benchmark.cpp
        source/ryu.cpp
        source/schubfach.cpp)

add_executable(benchmark ${benchmark_headers} ${benchmark_sources})

target_compile_features(benchmark PRIVATE cxx_std_17)

target_include_directories(benchmark
        PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)

target_link_libraries(benchmark
        PRIVATE
        ryu::ryu
        dragonbox::common
        dragonbox::dragonbox_to_chars
        grisu_exact::grisu_exact
        schubfach::schubfach)

# ---- MSVC Specifics ----
if (MSVC)
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/16478
    set_target_properties(benchmark PROPERTIES 
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif()
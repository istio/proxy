cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(dragonboxTests LANGUAGES CXX)

# ---- Add dependencies ----

# We want warnings in tests
set(dragonbox_INCLUDE_WITHOUT_SYSTEM ON CACHE INTERNAL "")

include(FetchContent)
if (NOT TARGET dragonbox)
    FetchContent_Declare(dragonbox SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
    FetchContent_MakeAvailable(dragonbox)
endif()
if (NOT TARGET common)
    FetchContent_Declare(common SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../common")
    FetchContent_MakeAvailable(common)
endif()
if (NOT TARGET simple)
    FetchContent_Declare(simple SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../simple")
    FetchContent_MakeAvailable(simple)
endif()
if (NOT TARGET ryu)
    FetchContent_Declare(ryu SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/ryu")
    FetchContent_MakeAvailable(ryu)
endif()

# ---- Tests ----

enable_testing()

function(add_test NAME)
  cmake_parse_arguments(TEST "TO_CHARS;RYU" "NAME" "" ${ARGN})
  if(TEST_TO_CHARS)
    set(dragonbox dragonbox::dragonbox_to_chars)
  else()
    set(dragonbox dragonbox::dragonbox)
  endif()

  add_executable(${NAME} source/${NAME}.cpp)

  target_link_libraries(${NAME} PRIVATE ${dragonbox} dragonbox::common dragonbox::simple)

  if(TEST_RYU)
    target_link_libraries(${NAME} PRIVATE ryu::ryu)
  endif()

  target_compile_features(${NAME} PRIVATE cxx_std_17)

  _add_test(NAME ${NAME} COMMAND ${NAME})

  # ---- MSVC Specifics ----
  if (MSVC)
      # See https://gitlab.kitware.com/cmake/cmake/-/issues/16478
      set_target_properties(${NAME} PROPERTIES 
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
  endif()
endfunction()

add_test(verify_cache_precision)
add_test(test_all_shorter_interval_cases TO_CHARS RYU)
add_test(uniform_random_test TO_CHARS RYU)
add_test(verify_compressed_cache)
add_test(verify_fast_multiplication)
add_test(verify_log_computation)
add_test(verify_magic_division)

option(DRAGONBOX_ENABLE_CONSTEXPR_TEST "Build constexpr test" OFF)
if (DRAGONBOX_ENABLE_CONSTEXPR_TEST)
    add_test(constexpr)
    set_target_properties(constexpr PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
    )
endif()

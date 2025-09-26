cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(dragonbox_meta LANGUAGES CXX)

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

function(meta_exe NAME)
  add_executable(${NAME} source/${NAME}.cpp)

  target_compile_features(${NAME} PRIVATE cxx_std_17)

  target_link_libraries(${NAME} PRIVATE ${ARGN})

  # ---- MSVC Specifics ----
  if (MSVC)
      # See https://gitlab.kitware.com/cmake/cmake/-/issues/16478
      set_target_properties(${NAME} PROPERTIES 
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
  endif()
endfunction()

meta_exe(generate_cache dragonbox::common)
meta_exe(live_test dragonbox::common dragonbox::dragonbox_to_chars ryu::ryu)
meta_exe(perf_test dragonbox::common dragonbox::dragonbox_to_chars)
meta_exe(sandbox dragonbox::common dragonbox::dragonbox_to_chars)
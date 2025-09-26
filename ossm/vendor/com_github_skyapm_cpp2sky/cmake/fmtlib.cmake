cmake_minimum_required(VERSION 3.14)

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x600)
endif()
 
find_package(Threads REQUIRED)

if(FMTLIB_AS_SUBMODULE)
  # using submodule in case of git clone timeout 
  if(CPP2SKY_INSTALL)
    set(FMT_INSTALL ON)
  endif(CPP2SKY_INSTALL)
  add_subdirectory(3rdparty/fmt ${CMAKE_CURRENT_BINARY_DIR}/fmt)
  message(STATUS "Using fmt via add_subdirectory.")
elseif(FMTLIB_FETCHCONTENT)
  # using FetchContent to install spdlog
  include(FetchContent)
  if(${CMAKE_VERSION} VERSION_LESS 3.14)
      include(add_FetchContent_MakeAvailable.cmake)
  endif()  

  FetchContent_Declare(
    fmtlib
    URL       https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip
    URL_HASH  SHA256=23778bad8edba12d76e4075da06db591f3b0e3c6c04928ced4a7282ca3400e5d
  )
  FetchContent_MakeAvailable(fmtlib)
else()
  find_package(fmt CONFIG REQUIRED)
  message(STATUS "Using fmt by find_package")
endif()

cmake_minimum_required(VERSION 3.14)

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x600)
endif()
 
find_package(Threads REQUIRED)

if(HTTPLIB_AS_SUBMODULE)
  # using submodule in case of git clone timeout 
  add_subdirectory(3rdparty/httplib ${CMAKE_CURRENT_BINARY_DIR}/httplib)
  message(STATUS "Using httplib via add_subdirectory.")
elseif(HTTPLIB_FETCHCONTENT)
  # using FetchContent to install spdlog
  include(FetchContent)
  if(${CMAKE_VERSION} VERSION_LESS 3.14)
      include(add_FetchContent_MakeAvailable.cmake)
  endif()  

  set(HTTPLIB_GIT_TAG  v0.7.15)
  set(HTTPLIB_GIT_URL  https://github.com/yhirose/cpp-httplib.git)

  FetchContent_Declare(
    httplib
    GIT_REPOSITORY    ${HTTPLIB_GIT_URL}
    GIT_TAG           ${HTTPLIB_GIT_TAG}
  )  

  FetchContent_MakeAvailable(httplib)
else()
  find_package(httplib CONFIG REQUIRED)
  message(STATUS "Using httplib by find_package")
endif()

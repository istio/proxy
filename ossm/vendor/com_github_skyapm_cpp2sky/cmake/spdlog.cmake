cmake_minimum_required(VERSION 3.14)

if(MSVC)
  add_definitions(-D_WIN32_WINNT=0x600)
endif()
 
find_package(Threads REQUIRED)

if(SPDLOG_AS_SUBMODULE)
  # using submodule in case of git clone timeout 
  if(CPP2SKY_INSTALL)
    set(SPDLOG_MASTER_PROJECT ON)
  endif(CPP2SKY_INSTALL)
  add_subdirectory(3rdparty/spdlog ${CMAKE_CURRENT_BINARY_DIR}/spdlog)
  message(STATUS "Using spdlog via add_subdirectory.")
elseif(SPDLOG_FETCHCONTENT)
  # using FetchContent to install spdlog
  include(FetchContent)
  if(${CMAKE_VERSION} VERSION_LESS 3.14)
      include(add_FetchContent_MakeAvailable.cmake)
  endif()  

  set(SPDLOG_GIT_TAG  v1.9.2)
  set(SPDLOG_GIT_URL  https://github.com/gabime/spdlog.git)

  FetchContent_Declare(
    spdlog
    GIT_REPOSITORY    ${SPDLOG_GIT_URL}
    GIT_TAG           ${SPDLOG_GIT_TAG}
  )  

  FetchContent_MakeAvailable(spdlog)
else()
  find_package(spdlog CONFIG REQUIRED)
  message(STATUS "Using spdlog by find_package")
endif()

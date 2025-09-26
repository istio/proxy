cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(dragonbox
        VERSION 1.1.3
        LANGUAGES CXX)

# ---- Includes ----

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# ---- Warning guard ----

# Protect dependents from this project's warnings if the guard isn't disabled
set(dragonbox_warning_guard "SYSTEM")
if(dragonbox_INCLUDE_WITHOUT_SYSTEM)
  set(dragonbox_warning_guard "")
endif()

# ---- Declare library (dragonbox) ----

set(dragonbox_headers include/dragonbox/dragonbox.h)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19.0")
	add_library(dragonbox INTERFACE ${dragonbox_headers})
else()
	add_library(dragonbox INTERFACE)
endif()	
add_library(dragonbox::dragonbox ALIAS dragonbox)

target_include_directories(dragonbox
        ${dragonbox_warning_guard}
        INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_compile_features(dragonbox INTERFACE cxx_std_17)

# ---- Declare library (dragonbox_to_chars) ----

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.19.0")
	set(dragonbox_to_chars_headers include/dragonbox/dragonbox_to_chars.h)
else()
	set(dragonbox_to_chars_headers
        	${dragonbox_headers}
        	include/dragonbox/dragonbox_to_chars.h)
endif()

set(dragonbox_to_chars_sources source/dragonbox_to_chars.cpp)

add_library(dragonbox_to_chars STATIC
        ${dragonbox_to_chars_headers}
        ${dragonbox_to_chars_sources})
add_library(dragonbox::dragonbox_to_chars ALIAS dragonbox_to_chars)

target_include_directories(dragonbox_to_chars
        ${dragonbox_warning_guard}
        PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_link_libraries(dragonbox_to_chars PUBLIC dragonbox)

target_compile_features(dragonbox_to_chars PUBLIC cxx_std_17)

# ---- Install ----

option(DRAGONBOX_INSTALL_TO_CHARS
        "When invoked with --install, dragonbox_to_chars.h/.cpp are installed along with dragonbox.h"
        On)

set(dragonbox_directory "dragonbox-${PROJECT_VERSION}")
set(dragonbox_include_directory "${CMAKE_INSTALL_INCLUDEDIR}/${dragonbox_directory}")
set(dragonbox_install_targets "dragonbox")

if (DRAGONBOX_INSTALL_TO_CHARS)
  set(dragonbox_install_targets ${dragonbox_install_targets} dragonbox_to_chars)
endif()

install(TARGETS ${dragonbox_install_targets}
        EXPORT dragonboxTargets
        ARCHIVE #
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT dragonbox_Development
        INCLUDES #
        DESTINATION "${dragonbox_include_directory}")

set(dragonbox_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/${dragonbox_directory}")

write_basic_package_version_file(
        dragonboxConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT)

install(EXPORT dragonboxTargets
        NAMESPACE dragonbox::
        DESTINATION "${dragonbox_install_cmakedir}")

install(FILES
        "${PROJECT_SOURCE_DIR}/cmake/dragonboxConfig.cmake"
        "${PROJECT_BINARY_DIR}/dragonboxConfigVersion.cmake"
        DESTINATION "${dragonbox_install_cmakedir}")

if (DRAGONBOX_INSTALL_TO_CHARS)
  install(FILES ${dragonbox_to_chars_headers}
          DESTINATION "${dragonbox_include_directory}/dragonbox")
else()
  install(FILES ${dragonbox_headers}
          DESTINATION "${dragonbox_include_directory}/dragonbox")
endif()

# ---- Subproject ----

option(DRAGONBOX_ENABLE_SUBPROJECT "Build subproject as well" OFF)

if (DRAGONBOX_ENABLE_SUBPROJECT)
    add_subdirectory("subproject/benchmark")
    add_subdirectory("subproject/meta")
    add_subdirectory("subproject/test")
endif()
macro(get_WIN32_WINNT version)
  if(CMAKE_SYSTEM_VERSION)
    set(ver ${CMAKE_SYSTEM_VERSION})
    string(REGEX MATCH "^([0-9]+).([0-9])" ver ${ver})
    string(REGEX MATCH "^([0-9]+)" verMajor ${ver})

    # Check for Windows 10, b/c we'll need to convert to hex 'A'.
    if("${verMajor}" MATCHES "10")
      set(verMajor "A")
      string(REGEX REPLACE "^([0-9]+)" ${verMajor} ver ${ver})
    endif()

    string(REPLACE "." "" ver ${ver})
    # Prepend each digit with a zero.
    string(REGEX REPLACE "([0-9A-Z])" "0\\1" ver ${ver})
    set(${version} "0x${ver}")
  endif()
endmacro()

get_WIN32_WINNT(win_ver)

# Automatically export all symbols
# It avoid the need for explicit dllimport/dllexport
#
# NOTE: when the library will have a public API. We should enforce
# which symbol to export (even with gcc)
set(WINDOWS_EXPORT_ALL_SYMBOLS ON)

# This target interface encapsulates compiler and linker options for the project.
# It provides a convenient way to apply these options to multiple targets.
# ----
# Usage: Link `dd_trace_cpp-specs` to targets using target_link_libraries.
add_library(dd_trace_cpp-specs INTERFACE)
add_library(dd_trace::specs ALIAS dd_trace_cpp-specs)

target_compile_options(dd_trace_cpp-specs
  INTERFACE
    /W4
    /wd4706
    /D_CRT_SECURE_NO_WARNINGS
    /D_WIN32_WINNT=${win_ver}
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug|RelWithDebInfo")
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      # Embedded debug information in binaries (no pdb)
      /Z7
  )
endif ()

if (DD_TRACE_ENABLE_COVERAGE)
  message(FATAL_ERROR "BUILD_COVERAGE is not supported for MSVC builds.")
endif ()

if (DD_TRACE_BUILD_FUZZERS)
  message(FATAL_ERROR "Fuzzers are not support for MSVC builds.")
endif ()

if (DD_TRACE_ENABLE_SANITIZE)
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      /fsanitize=address
      /RTC1
  )
  target_link_options(dd_trace_cpp-specs
    INTERFACE
      /fsanitize=address
  )
endif ()


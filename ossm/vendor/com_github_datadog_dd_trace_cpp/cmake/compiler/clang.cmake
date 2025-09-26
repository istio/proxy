# This target interface encapsulates compiler and linker options for the project.
# It provides a convenient way to apply these options to multiple targets.
# ----
# Usage: Link `dd_trace_cpp-specs` to targets using target_link_libraries.
add_library(dd_trace_cpp-specs INTERFACE)
add_library(dd_trace::specs ALIAS dd_trace_cpp-specs)

target_compile_options(dd_trace_cpp-specs
  INTERFACE
    -Wall
    -Wextra
    # -Wformat
    # -Wformat=2
    # -Wconversion
    # -Wsign-conversion
    -Wimplicit-fallthrough
    -Werror
    -Werror=format-security
    # This warning has a false positive with clang. See
    # <https://stackoverflow.com/questions/52416362>.
    -Wno-error=unused-lambda-capture
    -fno-omit-frame-pointer
    -fno-delete-null-pointer-checks
    -fno-strict-overflow 
    -fno-strict-aliasing 
    -pedantic
)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 16)
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
    -fstrict-flex-arrays=3
  )
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release|RelWithDebInfo")
  # -ftrivial-auto-var-init can interfere with other tools
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
    -ftrivial-auto-var-init=zero
  )
endif ()

function(add_sanitizers)
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      -fsanitize=address,undefined
  )

  target_link_libraries(dd_trace_cpp-specs
    INTERFACE
      -fsanitize=address,undefined
  )
endfunction()

if (DD_TRACE_ENABLE_COVERAGE)
  find_program(GCOV_PATH gcov)
  if (NOT GCOV_PATH)
    message(FATAL_ERROR "gcov not found. Cannot build with -DDD_TRACE_ENABLE_COVERAGE")
  endif ()

  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      -g
      -O0
      -fprofile-arcs
      -ftest-coverage
  )

  target_link_libraries(dd_trace_cpp-specs
    INTERFACE
      -fprofile-arcs
  )
endif()

if (DD_TRACE_ENABLE_SANITIZE)
  add_sanitizers()
endif()

if (DD_TRACE_BUILD_FUZZERS)
  add_sanitizers()

  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      -fsanitize=fuzzer
  )

  target_link_libraries(dd_trace_cpp-specs
    INTERFACE
      -fsanitize=fuzzer
  )
else ()
  # If we're building with clang, then use the libc++ version of the standard
  # library instead of libstdc++. Better coverage of build configurations.
  #
  # But there's one exception: libfuzzer is built with libstdc++ on Ubuntu,
  # and so won't link to libc++. So, if any of the FUZZ_* variables are set,
  # keep to libstdc++ (the default on most systems).
  message(STATUS "C++ standard library: libc++")
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()


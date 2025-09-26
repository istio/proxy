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
    -Wtrampolines
    -Wimplicit-fallthrough
    -Werror
    -Werror=format-security
    # This warning has a false positive. See
    # <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108088>.
    -Wno-error=free-nonheap-object
    -fno-omit-frame-pointer
    -fno-delete-null-pointer-checks
    -fno-strict-overflow 
    -fno-strict-aliasing 
    -pedantic
)

target_link_options(dd_trace_cpp-specs
  INTERFACE
    -Wl,-z,noexecstack
)

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 13)
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      -Wbidi-chars=any
      -fstrict-flex-arrays=3
  )

  if (CMAKE_BUILD_TYPE STREQUAL "Release|RelWithDebInfo")
    # -ftrivial-auto-var-init can interfere with other tools
    target_compile_options(dd_trace_cpp-specs
      INTERFACE
        -ftrivial-auto-var-init=zero
    )
  endif ()
endif ()

if (DD_TRACE_ENABLE_COVERAGE)
  find_program(GCOV_PATH gcov)
  if (NOT GCOV_PATH)
    message(FATAL_ERROR "gcov not found. Cannot build with -DDD_TRACE_ENABLE_COVERAGE")
  endif ()

  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      -g -O0 -fprofile-arcs -ftest-coverage
  )

  target_link_libraries(dd_trace_cpp-specs
    INTERFACE
      -fprofile-arcs
  )
endif()

if (DD_TRACE_ENABLE_SANITIZE)
  target_compile_options(dd_trace_cpp-specs
    INTERFACE
      -fsanitize=address,undefined
  )

  target_link_libraries(dd_trace_cpp-specs
    INTERFACE
      -fsanitize=address,undefined
  )
endif()

if (DD_TRACE_BUILD_FUZZERS)
  message(FATAL_ERROR "Fuzzers are not support for GCC builds")
endif ()


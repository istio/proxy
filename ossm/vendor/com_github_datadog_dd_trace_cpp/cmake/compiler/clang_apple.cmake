include(cmake/compiler/clang.cmake)

find_library(COREFOUNDATION_LIBRARY CoreFoundation)
find_library(SYSTEMCONFIGURATION_LIBRARY SystemConfiguration)

target_link_libraries(dd_trace_cpp-specs
  INTERFACE
    ${COREFOUNDATION_LIBRARY}
    ${SYSTEMCONFIGURATION_LIBRARY}
)

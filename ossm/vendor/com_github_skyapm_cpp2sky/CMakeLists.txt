cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW) # recognize CMAKE_MSVC_RUNTIME_LIBRARY
endif()

project(cpp2sky
    VERSION 0.5.1
    DESCRIPTION "Distributed tracing and monitor SDK in CPP for Apache SkyWalking APM"
    HOMEPAGE_URL "https://github.com/SkyAPM/cpp2sky"
)

option(OVERRIDE_CXX_STANDARD_FLAGS "Force building with -std=c++11 even if the CXXFLAGS are configured differently" ON)
option(SPDLOG_FETCHCONTENT         "Using spdlog FetchContent to build" ON)
option(FMTLIB_FETCHCONTENT         "Using fmt FetchContent to build" ON)
option(HTTPLIB_FETCHCONTENT        "Using httplib FetchContent to build" ON)
option(CPP2SKY_INSTALL             "Generate the install target." OFF)
option(GENERATE_CPP2SKY_PKGCONFIG  "Generate and install pkg-config files for UNIX" OFF)


if(OVERRIDE_CXX_STANDARD_FLAGS)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(GNUInstallDirs)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)


file(GLOB_RECURSE SOURCE_CC_FILES "source/*.cc")
file(GLOB_RECURSE SOURCE_HDR_FILES "source/*.h")
add_library(${PROJECT_NAME} STATIC ${SOURCE_CC_FILES} ${SOURCE_HDR_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>  
)

add_executable(example-sample "example/sample.cc")
add_executable(example-sample-client "example/sample_client.cc")
target_link_libraries(example-sample ${PROJECT_NAME})
target_link_libraries(example-sample-client ${PROJECT_NAME})


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(fmtlib)
include(spdlog)
include(httplib)
include(grpc)
include(proto2cpp)

target_link_libraries(${PROJECT_NAME}
    PUBLIC $<BUILD_INTERFACE:proto_lib>
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    fmt
    spdlog
    $<$<BOOL:${HTTPLIB_FETCHCONTENT}>:httplib>
)


if(CPP2SKY_INSTALL)
    include(CMakePackageConfigHelpers)
    # Install the static library.
    install(
        TARGETS  ${PROJECT_NAME} 
        EXPORT   ${PROJECT_NAME}-targets
        RUNTIME  DESTINATION  ${CMAKE_INSTALL_BINDIR}
        LIBRARY  DESTINATION  ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION  ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION  ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install export
    install(
        EXPORT ${PROJECT_NAME}-targets
        NAMESPACE ${PROJECT_NAME}::
        FILE ${PROJECT_NAME}-targets.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )

    # Install the project include
    install(DIRECTORY cpp2sky 
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
            PATTERN "*.h"
            PATTERN "BUILD" EXCLUDE
            PATTERN "*.proto" EXCLUDE
    )

    # Install *.cmake.in
    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/cpp2sky-config.cmake.in
        ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )

    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    )
endif()


if(GENERATE_CPP2SKY_PKGCONFIG)
      # see https://github.com/jupp0r/prometheus-cpp/issues/587
    if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
        set(CPP2SKY_PKGCONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
        set(CPP2SKY_PKGCONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()

    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(CPP2SKY_PKGCONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    else()
        set(CPP2SKY_PKGCONFIG_LIBDIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif()
    configure_file(
        ${PROJECT_SOURCE_DIR}/cmake/cpp2sky.pc.in
        ${PROJECT_BINARY_DIR}/cpp2sky.pc
        @ONLY
    )

    # Install the pkgconfig
    install(
        FILES ${PROJECT_BINARY_DIR}/cpp2sky.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )

endif()
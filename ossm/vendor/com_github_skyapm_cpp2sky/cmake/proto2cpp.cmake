#[[
function PROTOBUF_GENERATE_CPP demo:

# case 01:
    ROOT_DIR = ${CMAKE_SOURCE_DIR}/3rdparty/skywalking-data-collect-protocol
    NEED_GRPC_SERVICE = ON
    ARGN = language-agent/*.proto;

# case 02:
    ROOT_DIR = ${CMAKE_SOURCE_DIR}/3rdparty/skywalking-data-collect-protocol
    NEED_GRPC_SERVICE = OFF
    ARGN = common/*.proto;
]]
function(PROTOBUF_GENERATE_CPP SRCS HDRS ROOT_DIR NEED_GRPC_SERVICE)
    if(NOT ARGN)
        message(SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files")
        return()
    endif()

    set(${SRCS})
    set(${HDRS})

    foreach(FIL ${ARGN})
        get_filename_component(FIL_WE ${FIL} NAME_WE)
        get_filename_component(FIL_DIR ${FIL} PATH)

        list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.pb.cc")
        list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.pb.h")

        add_custom_command(
            OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.pb.cc"
                    "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.pb.h"
            COMMAND ${_PROTOBUF_PROTOC}
            ARGS    ${FIL} 
                    --cpp_out  ${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol
                    -I ${ROOT_DIR}
            COMMENT "Running Protocol Buffer Compiler on ${FIL}"
            VERBATIM 
            )

        if(NEED_GRPC_SERVICE)
            list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.grpc.pb.cc")
            list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.grpc.pb.h")

            add_custom_command(
                OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.grpc.pb.cc"
                        "${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol/${FIL_DIR}/${FIL_WE}.grpc.pb.h"
                COMMAND ${_PROTOBUF_PROTOC}
                ARGS    ${FIL}  
                        --grpc_out  ${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol
                        --plugin=protoc-gen-grpc=${_GRPC_CPP_PLUGIN_EXECUTABLE}
                        -I ${ROOT_DIR}
                COMMENT "Running C++ protocol service compiler on ${msg}"
                VERBATIM
                )
        endif(NEED_GRPC_SERVICE)
        
    endforeach()

    set_source_files_properties(${TMP_SRCS} ${TMP_HDRS} PROPERTIES GENERATED TRUE)
    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

# BaseRootDir
SET(SKYWALKING_PROTOCOL_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol)
if(EXISTS ${SKYWALKING_PROTOCOL_OUTPUT_DIR} AND IS_DIRECTORY ${SKYWALKING_PROTOCOL_OUTPUT_DIR})
        SET(PROTO_META_BASE_DIR ${SKYWALKING_PROTOCOL_OUTPUT_DIR})
else()
        file(MAKE_DIRECTORY ${SKYWALKING_PROTOCOL_OUTPUT_DIR})
        SET(PROTO_META_BASE_DIR ${SKYWALKING_PROTOCOL_OUTPUT_DIR})
endif()

# compile skywalking-data-collect-protocol/*.proto
set(NEED_GRPC_SERVICE ON)
set(PROTOC_FILES language-agent/Tracing.proto language-agent/ConfigurationDiscoveryService.proto)
set(PROTOC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/skywalking-data-collect-protocol")
PROTOBUF_GENERATE_CPP(SERVICE_PROTO_SRCS SERVICE_PROTO_HDRS ${PROTOC_BASE_DIR} ${NEED_GRPC_SERVICE} ${PROTOC_FILES})

set(NEED_GRPC_SERVICE OFF)
set(PROTOC_FILES common/Common.proto)
set(PROTOC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/skywalking-data-collect-protocol")
PROTOBUF_GENERATE_CPP(SDC_PROTO_SRCS SDC_PROTO_HDRS ${PROTOC_BASE_DIR} ${NEED_GRPC_SERVICE} ${PROTOC_FILES})


# compile config.proto
set(NEED_GRPC_SERVICE OFF)
set(PROTOC_FILES cpp2sky/config.proto)
set(PROTOC_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
PROTOBUF_GENERATE_CPP(CONFIG_PROTO_SRCS CONFIG_PROTO_HDRS ${PROTOC_BASE_DIR} ${NEED_GRPC_SERVICE} ${PROTOC_FILES})

add_library(proto_lib STATIC 
    ${SDC_PROTO_SRCS} 
    ${SDC_PROTO_HDRS}
    ${CONFIG_PROTO_SRCS} 
    ${CONFIG_PROTO_HDRS}
    ${SERVICE_PROTO_SRCS}
    ${SERVICE_PROTO_HDRS}
)
target_include_directories(proto_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol)

install(TARGETS proto_lib 
    RUNTIME  DESTINATION  ${CMAKE_INSTALL_BINDIR}
    LIBRARY  DESTINATION  ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE  DESTINATION  ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION  ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/skywalking-protocol 
DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.cc" EXCLUDE)

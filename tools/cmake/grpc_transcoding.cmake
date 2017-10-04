include_directories(
        vendor/grpc_transcoding/src/include
)

set(TANSCODING_SOURCES
        vendor/grpc_transcoding/src/http_template.cc
        vendor/grpc_transcoding/src/json_request_translator.cc
        vendor/grpc_transcoding/src/message_reader.cc
        vendor/grpc_transcoding/src/message_stream.cc
        vendor/grpc_transcoding/src/path_matcher_node.cc
        vendor/grpc_transcoding/src/prefix_writer.cc
        vendor/grpc_transcoding/src/request_message_translator.cc
        vendor/grpc_transcoding/src/request_stream_translator.cc
        vendor/grpc_transcoding/src/request_weaver.cc
        vendor/grpc_transcoding/src/response_to_json_translator.cc
        vendor/grpc_transcoding/src/type_helper.cc
        )

add_library(grpc_transcoding STATIC ${TANSCODING_SOURCES})

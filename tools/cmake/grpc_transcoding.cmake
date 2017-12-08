
add_library(grpc_transcoding STATIC
        ${ISTIO_NATIVE}/grpc_transcoding/src/http_template.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/json_request_translator.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/message_reader.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/message_stream.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/path_matcher_node.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/prefix_writer.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/request_message_translator.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/request_stream_translator.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/request_weaver.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/response_to_json_translator.cc
        ${ISTIO_NATIVE}/grpc_transcoding/src/type_helper.cc
        )

target_include_directories(grpc_transcoding
        PRIVATE
            ${ISTIO_NATIVE}/grpc_transcoding/src/include
            ${ISTIO_NATIVE}/protobuf/src
)

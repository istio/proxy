set(ISTIOPROXY_SOURCES
        ${ISTIO_PROXY}/src/envoy/mixer/http_filter.cc
        ${ISTIO_PROXY}/src/envoy/mixer/utils.cc
        ${ISTIO_PROXY}/src/envoy/mixer/tcp_filter.cc
        ${ISTIO_PROXY}/src/envoy/mixer/mixer_control.cc
        ${ISTIO_PROXY}/src/envoy/mixer/config.cc
        ${ISTIO_PROXY}/src/envoy/mixer/grpc_transport.cc
        )


# Must be linked in, so OBJECT. 'static' will not be loaded since there
# are no references.
add_library(istioproxy OBJECT ${ISTIOPROXY_SOURCES})

target_include_directories(istioproxy
        PRIVATE ${ISTIO_NATIVE}/mixerclient
        ${ISTIO_GENFILES}/external/com_lyft_protoc_gen_validate
        ${ISTIO_NATIVE}/fmt
        ${ISTIO_NATIVE}/xxhash
        ${ISTIO_NATIVE}/api
        ${ISTIO_NATIVE}/envoy/include
        ${ISTIO_NATIVE}/proxy
        ${ISTIO_PROXY}
        ${ISTIO_GENFILES}/external/mixerapi_git
        ${ISTIO_GENFILES}/external/googleapis_git
        ${ISTIO_GENFILES}/external/googleapis
        ${ISTIO_GENFILES}/external/com_github_googleapis_googleapis
        ${ISTIO_GENFILES}/external/gogoproto_git
        ${ISTIO_GENFILES}
        ${ISTIO_NATIVE}/boringssl/src/include
        ${ISTIO_NATIVE}/envoy/source
        ${ISTIO_NATIVE}/spdlog/include
        ${ISTIO_GENFILES}/external/envoy_api
        ${ISTIO_NATIVE}/lightstep/src/c++11
        ${ISTIO_NATIVE}/protobuf/src
        )

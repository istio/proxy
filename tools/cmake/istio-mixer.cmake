
set(ISTIOMIXER_SOURCES
        ${ISTIO_NATIVE}/mixerclient/control/src/http/attributes_builder.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/http/client_context.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/http/controller_impl.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/http/request_handler_impl.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/http/service_context.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/tcp/attributes_builder.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/tcp/controller_impl.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/tcp/request_handler_impl.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/attribute_names.cc
        ${ISTIO_NATIVE}/mixerclient/control/src/client_context_base.cc

        ${ISTIO_NATIVE}/mixerclient/control/src/utils/status.cc
        ${ISTIO_NATIVE}/mixerclient/src/attribute_compressor.cc
        ${ISTIO_NATIVE}/mixerclient/src/attributes_builder.cc
        ${ISTIO_NATIVE}/mixerclient/src/check_cache.cc
        # CLang build errors
        ${ISTIO_NATIVE}/mixerclient/src/referenced.cc
        ${ISTIO_NATIVE}/mixerclient/src/client_impl.cc
        ${ISTIO_NATIVE}/mixerclient/src/delta_update.cc
        ${ISTIO_NATIVE}/mixerclient/src/quota_cache.cc
        ${ISTIO_NATIVE}/mixerclient/utils/md5.cc
        ${ISTIO_NATIVE}/mixerclient/utils/protobuf.cc
        ${ISTIO_NATIVE}/mixerclient/src/report_batch.cc
        ${ISTIO_NATIVE}/mixerclient/prefetch/quota_prefetch.cc
        ${ISTIO_NATIVE}/mixerclient/prefetch/time_based_counter.cc

        ${ISTIO_NATIVE}/mixerclient/quota/src/config_parser_impl.cc
        ${ISTIO_NATIVE}/mixerclient/api_spec/src/http_api_spec_parser_impl.cc
        ${ISTIO_NATIVE}/mixerclient/api_spec/src/http_template.cc
        ${ISTIO_NATIVE}/mixerclient/api_spec/src/path_matcher_node.cc

        ${ISTIO_GENFILES}/external/gogoproto_git/gogoproto/gogo.pb.cc

        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/config/client/api_spec.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/config/client/auth.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/config/client/client_config.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/config/client/quota.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/config/client/service.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/attributes.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/check.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/report.pb.cc
        ${ISTIO_GENFILES}/external/mixerapi_git/mixer/v1/service.pb.cc
        ${ISTIO_GENFILES}/external/googleapis_git/google/rpc/status.pb.cc
        #${ISTIO_GENFILES}/src/envoy/mixer/string_map.pb.cc
        ${ISTIO_GENFILES}/external/mixerclient_git/src/global_dictionary.cc
        )


add_library(istiomixer STATIC ${ISTIOMIXER_SOURCES})

target_include_directories(istiomixer
        PRIVATE ${ISTIO_NATIVE}/mixerclient
        ${ISTIO_GENFILES}/external/com_lyft_protoc_gen_validate
        ${ISTIO_NATIVE}/fmt
        ${ISTIO_NATIVE}/xxhash
        ${ISTIO_NATIVE}/api
        ${ISTIO_NATIVE}/envoy/include
        ${ISTIO_GENFILES}/external/mixerapi_git
        ${ISTIO_GENFILES}/external/googleapis_git
        ${ISTIO_GENFILES}/external/gogoproto_git
        ${ISTIO_GENFILES}
        ${ISTIO_NATIVE}/boringssl/src/include
        ${ISTIO_NATIVE}/envoy/source
        ${ISTIO_NATIVE}/spdlog/include
        ${ISTIO_GENFILES}/external/envoy_api
        ${ISTIO_NATIVE}/lightstep/src/c++11
        ${ISTIO_NATIVE}/protobuf/src
        )

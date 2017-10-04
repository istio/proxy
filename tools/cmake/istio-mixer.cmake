include_directories(
        vendor/mixerclient
        .
        vendor/api
)

set(ISTIOPROXY_SOURCES
        vendor/mixerclient/src/attribute.cc
        vendor/mixerclient/src/check_cache.cc
        vendor/mixerclient/src/referenced.cc
        vendor/mixerclient/src/client_impl.cc
        vendor/mixerclient/src/delta_update.cc
        vendor/mixerclient/src/quota_cache.cc
        vendor/mixerclient/utils/md5.cc
        vendor/mixerclient/utils/protobuf.cc
        vendor/mixerclient/src/report_batch.cc
        vendor/mixerclient/src/attribute_converter.cc
        vendor/mixerclient/prefetch/quota_prefetch.cc
        vendor/mixerclient/prefetch/time_based_counter.cc

        src/envoy/mixer/http_filter.cc
        src/envoy/mixer/utils.cc
        src/envoy/mixer/tcp_filter.cc
        src/envoy/mixer/mixer_control.cc
        src/envoy/mixer/config.cc
        src/envoy/mixer/grpc_transport.cc

        genfiles/mixer/v1/attributes.pb.cc
        genfiles/mixer/v1/check.pb.cc
        genfiles/mixer/v1/report.pb.cc
        genfiles/mixer/v1/service.pb.cc
        genfiles/google/rpc/status.pb.cc
        genfiles/src/envoy/mixer/string_map.pb.cc
        genfiles/global_dictionary.cc
        )


# Must be linked in, so OBJECT. 'static' will not be loaded since there
# are no references.
add_library(istioproxy OBJECT ${ISTIOPROXY_SOURCES})

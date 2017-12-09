set(ENVOY_SOURCE_FILES
        ${ISTIO_NATIVE}/envoy/source/common/common/to_lower_table.cc
        ${ISTIO_NATIVE}/envoy/source/common/runtime/runtime_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/runtime/uuid_util.cc

        ${ISTIO_NATIVE}/envoy/source/common/access_log/access_log_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/api/api_impl.cc

        ${ISTIO_NATIVE}/envoy/source/common/buffer/buffer_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/buffer/watermark_buffer.cc
        ${ISTIO_NATIVE}/envoy/source/common/buffer/zero_copy_input_stream_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/base64.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/hex.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/logger.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/thread.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/version.cc
        ${ISTIO_NATIVE}/envoy/source/common/common/version_linkstamp.cc
        #${ISTIO_NATIVE}/envoy/source/common/dynamo/dynamo_filter.cc
        #${ISTIO_NATIVE}/envoy/source/common/dynamo/dynamo_request_parser.cc
        #${ISTIO_NATIVE}/envoy/source/common/dynamo/dynamo_utility.cc
        #${ISTIO_NATIVE}/envoy/source/server/config/http/dynamo.cc
        #${ISTIO_NATIVE}/envoy/source/server/config/network/mongo_proxy.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/address_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/base_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/bootstrap_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/cds_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/filter_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/grpc_mux_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/lds_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/protocol_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/rds_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/tls_context_json.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/well_known_names.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/metadata.cc
        ${ISTIO_NATIVE}/envoy/source/common/config/utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/dispatched_thread.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/dispatcher_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/event_impl_base.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/file_event_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/libevent.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/signal_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/event/timer_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/filesystem/filesystem_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/filesystem/inotify/watcher_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/filter/auth/client_ssl.cc
        ${ISTIO_NATIVE}/envoy/source/common/filter/echo.cc
        ${ISTIO_NATIVE}/envoy/source/common/filter/ratelimit.cc
        ${ISTIO_NATIVE}/envoy/source/common/filter/tcp_proxy.cc
        ${ISTIO_NATIVE}/envoy/source/common/grpc/codec.cc
        ${ISTIO_NATIVE}/envoy/source/common/grpc/common.cc
        ${ISTIO_NATIVE}/envoy/source/common/grpc/http1_bridge_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/grpc/grpc_web_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/grpc/json_transcoder_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/grpc/transcoder_input_stream_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/access_log/access_log_formatter.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/access_log/access_log_impl.cc
        #${ISTIO_NATIVE}/envoy/source/common/access_log/access_log_formatter.cc
        #${ISTIO_NATIVE}/envoy/source/common/access_log/access_log_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/filter/buffer_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/filter/fault_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/filter/ip_tagging_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/filter/ratelimit.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/http1/codec_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/http1/conn_pool.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/http2/codec_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/http2/conn_pool.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/websocket/ws_handler_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/async_client_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/codec_client.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/codes.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/filter_utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/conn_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/conn_manager_utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/date_provider_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/header_map_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/message_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/rest_api_fetcher.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/user_agent.cc
        ${ISTIO_NATIVE}/envoy/source/common/http/utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/json/config_schemas.cc
        ${ISTIO_NATIVE}/envoy/source/common/json/json_loader.cc
        ${ISTIO_NATIVE}/envoy/source/common/memory/stats.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/address_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/cidr_range.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/connection_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/dns_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/filter_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/listen_socket_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/listener_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/proxy_protocol.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/resolver_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/network/utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/profiler/profiler.cc
        ${ISTIO_NATIVE}/envoy/source/common/protobuf/utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/redis/codec_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/redis/command_splitter_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/redis/conn_pool_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/redis/proxy_filter.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/config_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/config_utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/rds_impl.cc
        #${ISTIO_NATIVE}/envoy/source/common/router/resp_header_parser.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/rds_subscription.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/req_header_formatter.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/retry_state_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/router.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/router_ratelimit.cc
        ${ISTIO_NATIVE}/envoy/source/common/router/shadow_writer_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/singleton/manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/stats/stats_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/stats/statsd.cc
        ${ISTIO_NATIVE}/envoy/source/common/stats/thread_local_store.cc
        ${ISTIO_NATIVE}/envoy/source/common/thread_local/thread_local_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/zipkin/span_buffer.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/zipkin/span_context.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/zipkin/tracer.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/zipkin/util.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/zipkin/zipkin_core_types.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/zipkin/zipkin_tracer_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/http_tracer_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/cds_api_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/cluster_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/health_checker_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/host_utility.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/load_balancer_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/logical_dns_cluster.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/outlier_detection_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/ring_hash_lb.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/upstream_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/cds_subscription.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/original_dst_cluster.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/load_stats_reporter.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/subset_lb.cc

        ${ISTIO_NATIVE}/envoy/source/common/upstream/eds.cc
        ${ISTIO_NATIVE}/envoy/source/common/upstream/sds_subscription.cc
        ${ISTIO_NATIVE}/envoy/source/common/ssl/connection_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/ssl/context_config_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/ssl/context_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/ssl/context_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/ratelimit/ratelimit_impl.cc
        ${ISTIO_NATIVE}/envoy/source/common/tracing/lightstep_tracer_impl.cc


        ${ISTIO_NATIVE}/envoy/source/server/config/http/buffer.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/http/fault.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/http/grpc_http1_bridge.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/http/ratelimit.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/http/grpc_web.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/http/ip_tagging.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/http/router.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/network/client_ssl_auth.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/network/echo.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/network/http_connection_manager.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/network/ratelimit.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/network/redis_proxy.cc
        ${ISTIO_NATIVE}/envoy/source/server/config/network/tcp_proxy.cc

        ${ISTIO_NATIVE}/envoy/source/server/http/admin.cc
        ${ISTIO_NATIVE}/envoy/source/server/http/health_check.cc
        ${ISTIO_NATIVE}/envoy/source/server/connection_handler_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/drain_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/guarddog_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/server.cc
        ${ISTIO_NATIVE}/envoy/source/server/init_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/lds_api.cc
        ${ISTIO_NATIVE}/envoy/source/server/lds_subscription.cc
        ${ISTIO_NATIVE}/envoy/source/server/worker_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/listener_manager_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/watchdog_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/options_impl.cc
        ${ISTIO_NATIVE}/envoy/source/server/configuration_impl.cc

        ${ISTIO_NATIVE}/envoy/source/server/config_validation/api.cc
        ${ISTIO_NATIVE}/envoy/source/server/config_validation/admin.cc
        ${ISTIO_NATIVE}/envoy/source/server/config_validation/async_client.cc
        ${ISTIO_NATIVE}/envoy/source/server/config_validation/cluster_manager.cc
        ${ISTIO_NATIVE}/envoy/source/server/config_validation/dispatcher.cc
        ${ISTIO_NATIVE}/envoy/source/server/config_validation/dns.cc
        ${ISTIO_NATIVE}/envoy/source/server/config_validation/server.cc

        ${ISTIO_NATIVE}/envoy/source/exe/main.cc
        ${ISTIO_NATIVE}/envoy/source/exe/main_common.cc

        ${ISTIO_GENFILES}/external/envoy_api/api/address.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/bootstrap.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/auth.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/cds.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/filter/http/http_connection_manager.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/http/buffer.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/http/health_check.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/http/fault.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/http/router.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/accesslog/accesslog.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/filter/accesslog.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/fault.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/filter/network/mongo_proxy.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/network/redis_proxy.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/network/tcp_proxy.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/network/http_connection_manager.pb.cc


        ${ISTIO_GENFILES}/external/envoy_api/api/discovery.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/lds.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/protocol.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/rds.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/sds.pb.cc

        #${ISTIO_GENFILES}/external/com_lyft_protoc_gen_validate/validate/validate.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/tls_context.pb.cc
        #${ISTIO_GENFILES}/external/envoy_api/api/filter/http_connection_manager.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/base.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/health_check.pb.cc
        ${ISTIO_GENFILES}/external/envoy_api/api/eds.pb.cc
        ${ISTIO_GENFILES}/external/envoy/source/common/ratelimit/ratelimit.pb.cc

        #${ISTIO_GENFILES}/external/googleapis_git/google/api/annotations.pb.cc
        #${ISTIO_GENFILES}/external/googleapis_git/google/api/http.pb.cc

        # TODO: move to separate cmake - the abseil cmake doesn't seem to work on ARM
        ${ISTIO_NATIVE}/abseil-cpp/absl/base/internal/spinlock_wait.cc
        )

# Not working/used on android or musl. The file installs a signal handler for backtraces.
if (NOT DEFINED ANDROID_TOOLCHAIN AND NOT DEFINED USE_MUSL)
    set(ENVOY_SOURCE_FILES
            ${ENVOY_SOURCE_FILES}

            ${ISTIO_NATIVE}/envoy/source/common/api/os_sys_calls_impl.cc
            ${ISTIO_NATIVE}/envoy/source/common/mongo/bson_impl.cc
            ${ISTIO_NATIVE}/envoy/source/common/mongo/codec_impl.cc
            ${ISTIO_NATIVE}/envoy/source/common/mongo/proxy.cc
            ${ISTIO_NATIVE}/envoy/source/common/mongo/utility.cc
            ${ISTIO_NATIVE}/envoy/source/exe/signal_action.cc
            )
else ()
    set(ENVOY_SOURCE_FILES
            ${ENVOY_SOURCE_FILES}
            build/contrib/patches/os_sys_calls_impl.cc
            )
endif ()

add_executable(envoy ${ENVOY_SOURCE_FILES} $<TARGET_OBJECTS:istioproxy>)

if (DEFINED ANDROID_TOOLCHAIN)
    target_link_libraries(envoy PUBLIC -llog -landroid)
    set_target_properties(envoy PROPERTIES COMPILE_FLAGS "-DSPDLOG_NO_UNWIND -DBUILD_SCM_STATUS=\\\"1\\\" -DBUILD_SCM_REVISION=\\\"2\\\" -D_GLIBCXX_REGEX_STATE_LIMIT")
    set(CMAKE_CXX_FLAGS " -DSPDLOG_NO_UNWIND ${CMAKE_CXX_FLAGS}")
else ()
    target_link_libraries(envoy PUBLIC -lrt -lpthread)
    set_target_properties(envoy PROPERTIES COMPILE_FLAGS "-DBUILD_SCM_STATUS=\\\"1\\\" -DBUILD_SCM_REVISION=\\\"2\\\" -D_GLIBCXX_REGEX_STATE_LIMIT")
endif ()

set(CMAKE_CXX_FLAGS " -DHAVE_LONG_LONG ${CMAKE_CXX_FLAGS}")

target_include_directories(envoy PRIVATE
        ${ISTIO_GENFILES}/external/com_lyft_protoc_gen_validate
        ${ISTIO_GENFILES}/external/envoy_api
        ${ISTIO_GENFILES}/external/envoy
        ${ISTIO_GENFILES}/external/googleapis_git
        ${ISTIO_GENFILES}/external/googleapis
        ${ISTIO_NATIVE}/nghttp2/lib/includes
        ${ISTIO_DEP_GENFILES}/

        ${ISTIO_DEP_GENFILES}/cares/

        ${CMAKE_BINARY_DIR}/src/libevent/include
        ${CMAKE_BINARY_DIR}/vendor/src/libevent/include

        ${ISTIO_NATIVE}/lightstep/src/c++11
        ${ISTIO_NATIVE}/protobuf/src
        ${ISTIO_NATIVE}/spdlog/include
        ${ISTIO_NATIVE}/envoy/source
        ${ISTIO_NATIVE}/cares
        ${ISTIO_NATIVE}/fmt
        ${ISTIO_NATIVE}/xxhash
        ${ISTIO_NATIVE}/libevent/compat
        ${ISTIO_NATIVE}/libevent/include
        # Generated by event native cmake
        ${ISTIO_NATIVE}/xxhash
        ${ISTIO_NATIVE}/boringssl/src/include
        ${ISTIO_NATIVE}/http-parser
        ${ISTIO_NATIVE}/rapidjson/include
        ${ISTIO_NATIVE}/yaml-cpp/include
        ${ISTIO_NATIVE}/tclap/include
        ${ISTIO_NATIVE}/backward
        ${ISTIO_NATIVE}/grpc_transcoding/src/include

        ${ISTIO_NATIVE}/envoy/include
        ${ISTIO_NATIVE}/abseil-cpp
        )

target_link_libraries(envoy PUBLIC tracer)
target_link_libraries(envoy PUBLIC http-parser)
target_link_libraries(envoy PUBLIC cares)
target_link_libraries(envoy PUBLIC ssl)
target_link_libraries(envoy PUBLIC crypto)
target_link_libraries(envoy PUBLIC nghttp2)
target_link_libraries(envoy PUBLIC event_pthreads_static)
target_link_libraries(envoy PUBLIC event_core_static)
target_link_libraries(envoy PUBLIC libprotobuf)
target_link_libraries(envoy PUBLIC yaml-cpp)
target_link_libraries(envoy PUBLIC grpc_transcoding)
target_link_libraries(envoy PUBLIC xxhash)


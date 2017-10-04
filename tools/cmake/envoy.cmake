
include_directories(
        vendor/spdlog/include
        envoy/source
        vendor/cares/cares
        vendor/fmt
        vendor/libevent/src/compat
        vendor/libevent/src/include
        ${CMAKE_BINARY_DIR}/vendor/libevent/src/include
        vendor/xxhash
        vendor/boringssl/src/include
        vendor/http-parser/src
        vendor/rapidjson/include
        vendor/yaml-cpp/include
        vendor/tclap/include
        vendor/backward
)

set(ENVOY_SOURCE_FILES
        envoy/source/common/common/to_lower_table.cc
        envoy/source/common/runtime/runtime_impl.cc
        envoy/source/common/runtime/uuid_util.cc

        envoy/source/common/access_log/access_log_manager_impl.cc
        envoy/source/common/api/api_impl.cc

        envoy/source/common/buffer/buffer_impl.cc
        envoy/source/common/buffer/watermark_buffer.cc
        envoy/source/common/buffer/zero_copy_input_stream_impl.cc
        envoy/source/common/common/base64.cc
        envoy/source/common/common/hex.cc
        envoy/source/common/common/logger.cc
        envoy/source/common/common/thread.cc
        envoy/source/common/common/utility.cc
        envoy/source/common/common/version.cc
        envoy/source/common/common/version_linkstamp.cc
        #envoy/source/common/dynamo/dynamo_filter.cc
        #envoy/source/common/dynamo/dynamo_request_parser.cc
        #envoy/source/common/dynamo/dynamo_utility.cc
        #envoy/source/server/config/http/dynamo.cc
        #envoy/source/server/config/network/mongo_proxy.cc
        envoy/source/common/config/address_json.cc
        envoy/source/common/config/base_json.cc
        envoy/source/common/config/bootstrap_json.cc
        envoy/source/common/config/cds_json.cc
        envoy/source/common/config/filter_json.cc
        envoy/source/common/config/grpc_mux_impl.cc
        envoy/source/common/config/lds_json.cc
        envoy/source/common/config/protocol_json.cc
        envoy/source/common/config/rds_json.cc
        envoy/source/common/config/tls_context_json.cc

        envoy/source/common/config/metadata.cc
        envoy/source/common/config/utility.cc
        envoy/source/common/event/dispatched_thread.cc
        envoy/source/common/event/dispatcher_impl.cc
        envoy/source/common/event/event_impl_base.cc
        envoy/source/common/event/file_event_impl.cc
        envoy/source/common/event/libevent.cc
        envoy/source/common/event/signal_impl.cc
        envoy/source/common/event/timer_impl.cc
        envoy/source/common/filesystem/filesystem_impl.cc
        envoy/source/common/filesystem/inotify/watcher_impl.cc
        envoy/source/common/filter/auth/client_ssl.cc
        envoy/source/common/filter/echo.cc
        envoy/source/common/filter/ratelimit.cc
        envoy/source/common/filter/tcp_proxy.cc
        envoy/source/common/grpc/codec.cc
        envoy/source/common/grpc/common.cc
        envoy/source/common/grpc/http1_bridge_filter.cc
        envoy/source/common/grpc/rpc_channel_impl.cc
        envoy/source/common/grpc/grpc_web_filter.cc
        envoy/source/common/grpc/json_transcoder_filter.cc
        envoy/source/common/grpc/transcoder_input_stream_impl.cc
        envoy/source/common/http/access_log/access_log_formatter.cc
        envoy/source/common/http/access_log/access_log_impl.cc
        envoy/source/common/http/filter/buffer_filter.cc
        envoy/source/common/http/filter/fault_filter.cc
        envoy/source/common/http/filter/ip_tagging_filter.cc
        envoy/source/common/http/filter/ratelimit.cc
        envoy/source/common/http/http1/codec_impl.cc
        envoy/source/common/http/http1/conn_pool.cc
        envoy/source/common/http/http2/codec_impl.cc
        envoy/source/common/http/http2/conn_pool.cc
        envoy/source/common/http/websocket/ws_handler_impl.cc
        envoy/source/common/http/async_client_impl.cc
        envoy/source/common/http/codec_client.cc
        envoy/source/common/http/codes.cc
        envoy/source/common/http/filter_utility.cc
        envoy/source/common/http/conn_manager_impl.cc
        envoy/source/common/http/conn_manager_utility.cc
        envoy/source/common/http/date_provider_impl.cc
        envoy/source/common/http/header_map_impl.cc
        envoy/source/common/http/message_impl.cc
        envoy/source/common/http/rest_api_fetcher.cc
        envoy/source/common/http/user_agent.cc
        envoy/source/common/http/utility.cc
        envoy/source/common/json/config_schemas.cc
        envoy/source/common/json/json_loader.cc
        envoy/source/common/memory/stats.cc
        envoy/source/common/network/address_impl.cc
        envoy/source/common/network/cidr_range.cc
        envoy/source/common/network/connection_impl.cc
        envoy/source/common/network/dns_impl.cc
        envoy/source/common/network/filter_manager_impl.cc
        envoy/source/common/network/listen_socket_impl.cc
        envoy/source/common/network/listener_impl.cc
        envoy/source/common/network/proxy_protocol.cc
        envoy/source/common/network/utility.cc
        envoy/source/common/profiler/profiler.cc
        envoy/source/common/protobuf/utility.cc
        envoy/source/common/redis/codec_impl.cc
        envoy/source/common/redis/command_splitter_impl.cc
        envoy/source/common/redis/conn_pool_impl.cc
        envoy/source/common/redis/proxy_filter.cc
        envoy/source/common/router/config_impl.cc
        envoy/source/common/router/config_utility.cc
        envoy/source/common/router/rds_impl.cc
        envoy/source/common/router/rds_subscription.cc
        envoy/source/common/router/req_header_formatter.cc
        envoy/source/common/router/retry_state_impl.cc
        envoy/source/common/router/router.cc
        envoy/source/common/router/router_ratelimit.cc
        envoy/source/common/router/shadow_writer_impl.cc
        envoy/source/common/singleton/manager_impl.cc
        envoy/source/common/stats/stats_impl.cc
        envoy/source/common/stats/statsd.cc
        envoy/source/common/stats/thread_local_store.cc
        envoy/source/common/thread_local/thread_local_impl.cc
        envoy/source/common/tracing/zipkin/span_buffer.cc
        envoy/source/common/tracing/zipkin/span_context.cc
        envoy/source/common/tracing/zipkin/tracer.cc
        envoy/source/common/tracing/zipkin/util.cc
        envoy/source/common/tracing/zipkin/zipkin_core_types.cc
        envoy/source/common/tracing/zipkin/zipkin_tracer_impl.cc
        envoy/source/common/tracing/http_tracer_impl.cc
        envoy/source/common/upstream/cds_api_impl.cc
        envoy/source/common/upstream/cluster_manager_impl.cc
        envoy/source/common/upstream/health_checker_impl.cc
        envoy/source/common/upstream/host_utility.cc
        envoy/source/common/upstream/load_balancer_impl.cc
        envoy/source/common/upstream/logical_dns_cluster.cc
        envoy/source/common/upstream/outlier_detection_impl.cc
        envoy/source/common/upstream/ring_hash_lb.cc
        envoy/source/common/upstream/upstream_impl.cc
        envoy/source/common/upstream/cds_subscription.cc
        envoy/source/common/upstream/original_dst_cluster.cc

        envoy/source/common/upstream/eds.cc
        envoy/source/common/upstream/sds_subscription.cc
        envoy/source/common/ssl/connection_impl.cc
        envoy/source/common/ssl/context_config_impl.cc
        envoy/source/common/ssl/context_impl.cc
        envoy/source/common/ssl/context_manager_impl.cc
        envoy/source/common/ratelimit/ratelimit_impl.cc
        envoy/source/common/tracing/lightstep_tracer_impl.cc


        envoy/source/server/config/http/buffer.cc
        envoy/source/server/config/http/fault.cc
        envoy/source/server/config/http/grpc_http1_bridge.cc
        envoy/source/server/config/http/ratelimit.cc
        envoy/source/server/config/http/grpc_web.cc
        envoy/source/server/config/http/ip_tagging.cc
        envoy/source/server/config/http/router.cc
        envoy/source/server/config/network/client_ssl_auth.cc
        envoy/source/server/config/network/echo.cc
        envoy/source/server/config/network/http_connection_manager.cc
        envoy/source/server/config/network/ratelimit.cc
        envoy/source/server/config/network/redis_proxy.cc
        envoy/source/server/config/network/tcp_proxy.cc

        envoy/source/server/http/admin.cc
        envoy/source/server/http/health_check.cc
        envoy/source/server/connection_handler_impl.cc
        envoy/source/server/drain_manager_impl.cc
        envoy/source/server/guarddog_impl.cc
        envoy/source/server/server.cc
        envoy/source/server/init_manager_impl.cc
        envoy/source/server/lds_api.cc
        envoy/source/server/lds_subscription.cc
        envoy/source/server/worker_impl.cc
        envoy/source/server/listener_manager_impl.cc
        envoy/source/server/watchdog_impl.cc
        envoy/source/server/options_impl.cc
        envoy/source/server/configuration_impl.cc

        envoy/source/server/config_validation/api.cc
        envoy/source/server/config_validation/admin.cc
        envoy/source/server/config_validation/async_client.cc
        envoy/source/server/config_validation/cluster_manager.cc
        envoy/source/server/config_validation/dispatcher.cc
        envoy/source/server/config_validation/dns.cc
        envoy/source/server/config_validation/server.cc

        envoy/source/exe/main.cc
        envoy/source/exe/main_common.cc

        genfiles/api/address.pb.cc
        genfiles/api/bootstrap.pb.cc
        genfiles/api/cds.pb.cc
        genfiles/api/discovery.pb.cc
        genfiles/api/lds.pb.cc
        genfiles/api/protocol.pb.cc
        genfiles/api/rds.pb.cc
        genfiles/api/tls_context.pb.cc
        genfiles/api/filter/http_connection_manager.pb.cc
        genfiles/api/base.pb.cc
        genfiles/api/health_check.pb.cc
        genfiles/api/eds.pb.cc
        genfiles/common/ratelimit/ratelimit.pb.cc
        genfiles/gogoproto/gogo.pb.cc
        genfiles/google/api/annotations.pb.cc
        genfiles/google/api/http.pb.cc
        )

# Not working/used on android or musl. The file installs a signal handler for backtraces.
if (NOT DEFINED ANDROID_TOOLCHAIN AND NOT DEFINED USE_MUSL)
   set(ENVOY_SOURCE_FILES
           ${ENVOY_SOURCE_FILES}
           envoy/source/common/api/os_sys_calls_impl.cc
           envoy/source/common/mongo/bson_impl.cc
           envoy/source/common/mongo/codec_impl.cc
           envoy/source/common/mongo/proxy.cc
           envoy/source/common/mongo/utility.cc
           envoy/source/exe/signal_action.cc
        )
else()
    set(ENVOY_SOURCE_FILES
            ${ENVOY_SOURCE_FILES}
            tools/cmake/os_sys_calls_impl.cc
            )
endif()

add_executable(envoy ${ENVOY_SOURCE_FILES} $<TARGET_OBJECTS:istioproxy>)

if (DEFINED ANDROID_TOOLCHAIN)
    target_link_libraries (envoy PUBLIC -llog -landroid)
endif()

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


# envoy/source/common/common/version_linkstamp.cc:5:35:
# main.cc
set_target_properties(envoy PROPERTIES COMPILE_FLAGS "-DBUILD_SCM_STATUS=\\\"1\\\" -DBUILD_SCM_REVISION=\\\"2\\\" -D_GLIBCXX_REGEX_STATE_LIMIT")


# Envoy requires boring - will not work with platform openssl
#target_link_libraries(envoy -lssl -lcrypt)

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "per_host_upstream_config",
    hdrs = [
        "per_host_upstream_config.h",
    ],
    srcs = [
        "per_host_upstream_config.cc",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/registry",
        "@envoy//include/envoy/http:codes_interface",
        "@envoy//include/envoy/http:conn_pool_interface",
        "@envoy//include/envoy/http:filter_interface",
        "@envoy//include/envoy/upstream:cluster_manager_interface",
        "@envoy//include/envoy/upstream:upstream_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/router:router_lib",
        "@envoy//source/common/upstream:load_balancer_lib",
        "@envoy//source/extensions/upstreams/http/http:upstream_request_lib",
        "@envoy//source/extensions/upstreams/http/tcp:upstream_request_lib",
    ],
)

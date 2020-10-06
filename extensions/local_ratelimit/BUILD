licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "local_ratelimit_lib",
    srcs = [
        "config.cc",
        "plugin.cc",
    ],
    hdrs = [
        "plugin.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//extensions/common:context",
        "//extensions/common:istio_dimensions",
        "//extensions/local_ratelimit/config/v1alpha1:local_ratelimit_config_cc_proto",
        "@proxy_wasm_cpp_host//:lib",
        "@proxy_wasm_cpp_sdk//contrib:contrib_lib",
    ],
)

licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "metadata_exchange_lib",
    srcs = [
        "config.cc",
    ],
    hdrs = [
        "config.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/extensions/common/wasm/null:null_lib",
    ],
)

licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)
load(
    "@envoy//bazel/wasm:wasm.bzl",
    "wasm_cc_binary",
)

envoy_package()

envoy_cc_library(
    name = "basic_auth",
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
        "//extensions/common:json_util",
        "@envoy//source/common/common:base64_lib",
        "@proxy_wasm_cpp_host//:lib",
    ],
)

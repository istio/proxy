licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "plugin_lib",
    srcs = [
        "plugin.cc",
    ],
    hdrs = [
        "plugin.h",
    ],
    copts = ["-DNULL_PLUGIN=1"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/extensions/common/wasm/null:null_plugin_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":plugin_lib",
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/extensions/common/wasm/null:null_plugin_lib",
    ],
)

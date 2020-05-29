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
        "//extensions/common:proto_util",
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/extensions/common/wasm:declare_property_cc_proto",
        "@envoy//source/extensions/common/wasm/null:null_plugin_lib",
    ],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "@com_google_protobuf//:wrappers_proto",
    ],
)

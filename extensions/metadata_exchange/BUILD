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
        ":config_cc_proto",
        "//extensions/common:context",
        "@envoy//source/common/common:base64_lib",
        "@envoy//source/extensions/common/wasm:declare_property_cc_proto",
        "@envoy//source/extensions/common/wasm/null:null_plugin_lib",
    ],
)

cc_proto_library(
    name = "config_cc_proto",
    visibility = ["//visibility:public"],
    deps = ["config_proto"],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    deps = [
        "@com_google_protobuf//:wrappers_proto",
    ],
)

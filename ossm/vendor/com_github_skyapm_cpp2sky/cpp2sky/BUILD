load("@rules_cc//cc:defs.bzl", "cc_library")
load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

licenses(["notice"])  # Apache 2

proto_library(
    name = "config_proto_lib",
    srcs = ["config.proto"],
)

cc_proto_library(
    name = "config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":config_proto_lib"],
)

cc_library(
    name = "cpp2sky_interface",
    hdrs = [
        "assert.h",
        "exception.h",
        "propagation.h",
        "time.h",
        "trace_log.h",
        "tracer.h",
        "tracing_context.h",
        "well_known_names.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":config_cc_proto",
    ],
)

cc_library(
    name = "cpp2sky_data_interface",
    hdrs = [
        "assert.h",
        "exception.h",
        "propagation.h",
        "time.h",
        "tracing_context.h",
        "well_known_names.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":config_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

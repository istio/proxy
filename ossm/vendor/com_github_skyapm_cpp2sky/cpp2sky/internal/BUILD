load("@rules_cc//cc:defs.bzl", "cc_library")
load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "async_client_interface",
    hdrs = [
        "async_client.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@skywalking_data_collect_protocol//language-agent:tracing_protocol_cc_grpc",
    ],
)

cc_library(
    name = "random_generator_interface",
    hdrs = [
        "random_generator.h",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "matcher_interface",
    hdrs = ["matcher.h"],
    visibility = ["//visibility:public"],
)

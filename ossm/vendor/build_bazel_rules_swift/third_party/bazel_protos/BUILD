load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

licenses(["notice"])

proto_library(
    name = "worker_protocol_proto",
    srcs = ["worker_protocol.proto"],
)

cc_proto_library(
    name = "worker_protocol_cc_proto",
    visibility = ["//tools/worker:__pkg__"],
    deps = [":worker_protocol_proto"],
)

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = [
        "//third_party:__pkg__",
    ],
)

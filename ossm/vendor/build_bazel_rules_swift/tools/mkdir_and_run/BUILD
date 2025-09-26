load("@bazel_skylib//rules:native_binary.bzl", "native_binary")

licenses(["notice"])

native_binary(
    name = "mkdir_and_run",
    src = select({
        "@bazel_tools//src/conditions:host_windows": "mkdir_and_run.cmd",
        "//conditions:default": "mkdir_and_run.sh",
    }),
    out = select({
        "@bazel_tools//src/conditions:host_windows": "mkdir_and_run.cmd",
        "//conditions:default": "mkdir_and_run.sh",
    }),
    visibility = ["//visibility:public"],
)

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = [
        "//tools:__pkg__",
    ],
)

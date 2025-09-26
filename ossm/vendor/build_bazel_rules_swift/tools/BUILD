package(default_visibility = ["//visibility:public"])

licenses(["notice"])

# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]) + [
        "//tools/common:for_bazel_tests",
        "//tools/mkdir_and_run:for_bazel_tests",
        "//tools/worker:for_bazel_tests",
    ],
    visibility = [
        "//:__pkg__",
    ],
)

config_setting(
    name = "msvc",
    flag_values = {
        "@bazel_tools//tools/cpp:compiler": "msvc-cl",
    },
    visibility = [
        "//:__subpackages__",
    ],
)

config_setting(
    name = "clang-cl",
    flag_values = {
        "@bazel_tools//tools/cpp:compiler": "clang-cl",
    },
    visibility = [
        "//:__subpackages__",
    ],
)

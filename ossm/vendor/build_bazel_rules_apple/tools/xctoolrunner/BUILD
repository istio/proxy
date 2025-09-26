load("//tools:binary_env.bzl", "binary_env")

licenses(["notice"])

py_binary(
    name = "xctoolrunner",
    srcs = ["xctoolrunner.py"],
    env = binary_env.common_python_utf8_env(),
    python_version = "PY3",
    srcs_version = "PY3",
    visibility = [
        "//apple/internal:__pkg__",
    ],
    deps = [":xctoolrunner_lib"],
)

py_library(
    name = "xctoolrunner_lib",
    srcs = ["xctoolrunner.py"],
    srcs_version = "PY3",
    deps = ["//tools/wrapper_common:execute"],
)

py_test(
    name = "xctoolrunner_test",
    srcs = ["xctoolrunner_test.py"],
    python_version = "PY3",
    tags = ["requires-darwin"],
    deps = [
        ":xctoolrunner_lib",
        "//:py_init_shim",
        "//tools/wrapper_common:execute",
    ],
)

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = [
        "//tools:__pkg__",
    ],
)

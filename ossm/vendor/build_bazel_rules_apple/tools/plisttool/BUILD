licenses(["notice"])

py_binary(
    name = "plisttool",
    srcs = ["plisttool.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    visibility = [
        "//apple/internal:__pkg__",
    ],
    deps = [":plisttool_lib"],
)

py_library(
    name = "plisttool_lib",
    srcs = [
        "plisttool.py",
    ],
    srcs_version = "PY3",
)

py_test(
    name = "plisttool_unittest",
    srcs = ["plisttool_unittest.py"],
    python_version = "PY3",
    tags = ["requires-darwin"],
    deps = [
        ":plisttool_lib",
        "//:py_init_shim",
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

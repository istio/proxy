licenses(["notice"])

py_binary(
    name = "clangrttool",
    srcs = ["clangrttool.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    visibility = [
        "//apple/internal:__pkg__",
    ],
)

py_test(
    name = "clangrttool_unittest",
    srcs = ["clangrttool_unittest.py"],
    python_version = "PY3",
    deps = [
        ":clangrttool",
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

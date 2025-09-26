licenses(["notice"])

py_library(
    name = "execute",
    srcs = ["execute.py"],
    srcs_version = "PY3",
    visibility = [
        "//tools/bitcode_strip:__pkg__",
        "//tools/codesigningtool:__pkg__",
        "//tools/imported_dynamic_framework_processor:__pkg__",
        "//tools/swift_stdlib_tool:__pkg__",
        "//tools/xcarchivetool:__pkg__",
        "//tools/xctoolrunner:__pkg__",
        "//tools/xctrunnertool:__pkg__",
    ],
)

py_test(
    name = "execute_test",
    srcs = ["execute_test.py"],
    python_version = "PY3",
    tags = ["requires-darwin"],
    deps = [
        ":execute",
        "//:py_init_shim",
    ],
)

py_library(
    name = "lipo",
    srcs = ["lipo.py"],
    srcs_version = "PY3",
    visibility = [
        "//tools:__subpackages__",
    ],
    deps = [":execute"],
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

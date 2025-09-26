licenses(["notice"])

sh_binary(
    name = "main_thread_checker_tool",
    srcs = ["main_thread_checker_tool.sh"],
    visibility = [
        "//apple/internal:__pkg__",
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

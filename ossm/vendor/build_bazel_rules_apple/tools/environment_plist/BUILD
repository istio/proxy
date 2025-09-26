licenses(["notice"])

sh_binary(
    name = "environment_plist",
    srcs = ["environment_plist.sh"],
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

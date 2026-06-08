# Consumed by Bazel integration tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]) + [
        "//third_party/bazel_protos:for_bazel_tests",
        "//third_party/rules_proto:for_bazel_tests",
    ],
    visibility = [
        "//:__pkg__",
    ],
)

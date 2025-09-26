package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

# Consumed by Bazel integration tests (such as those defined in rules_apple).
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = [
        "WORKSPACE",
        "//swift:for_bazel_tests",
        "//third_party:for_bazel_tests",
        "//tools:for_bazel_tests",
    ],
)

package(default_visibility = ["//visibility:public"])

# Files which shouldn't be publicly visible and dependencies of all objc_* or ios_* rules should be excluded.
exports_files(glob(
    ["**"],
))

cc_binary(
    name = "wrapped_clang",
    testonly = True,
    srcs = [
        "wrapped_clang.cc",
    ],
    copts = ["-std=c++17"],
)

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = True,
    srcs = glob(["**"]),
    visibility = ["//:__pkg__"],
)

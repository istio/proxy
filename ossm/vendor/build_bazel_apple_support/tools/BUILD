licenses(["notice"])

# Additional files, such as tools, that should be present in a test's runfiles
# during coverage collection.
filegroup(
    name = "coverage_support",
    # Public so other rules can add the dependencies via this group.
    visibility = ["//visibility:public"],
)

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = 1,
    srcs = glob(["**"]),
    visibility = ["//:__pkg__"],
)

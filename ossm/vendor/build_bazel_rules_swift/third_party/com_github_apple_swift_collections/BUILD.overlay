load("@build_bazel_rules_swift//swift:swift_library.bzl", "swift_library")

swift_library(
    name = "Collections",
    srcs = glob([
        "Sources/Collections/**/*.swift",
    ]),
    module_name = "Collections",
    visibility = ["//visibility:public"],
    deps = [
        ":DequeModule",
        ":OrderedCollections",
    ],
)

swift_library(
    name = "DequeModule",
    srcs = glob([
        "Sources/DequeModule/**/*.swift",
    ]),
    module_name = "DequeModule",
    visibility = ["//visibility:public"],
)

swift_library(
    name = "OrderedCollections",
    srcs = glob([
        "Sources/OrderedCollections/**/*.swift",
    ]),
    module_name = "OrderedCollections",
    visibility = ["//visibility:public"],
)

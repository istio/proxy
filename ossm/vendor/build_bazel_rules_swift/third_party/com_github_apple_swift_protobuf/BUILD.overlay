load("@build_bazel_rules_swift//swift:swift_binary.bzl", "swift_binary")
load("@build_bazel_rules_swift//swift:swift_library.bzl", "swift_library")

swift_library(
    name = "SwiftProtobuf",
    srcs = glob([
        "Sources/SwiftProtobuf/*.swift",
    ]),
    module_name = "SwiftProtobuf",
    visibility = ["//visibility:public"],
)

swift_library(
    name = "SwiftProtobufPluginLibrary",
    srcs = glob([
        "Sources/SwiftProtobufPluginLibrary/*.swift",
    ]),
    module_name = "SwiftProtobufPluginLibrary",
    visibility = ["//visibility:public"],
    deps = [":SwiftProtobuf"],
)

swift_binary(
    name = "ProtoCompilerPlugin",
    srcs = glob([
        "Sources/protoc-gen-swift/*.swift",
    ]),
    visibility = ["//visibility:public"],
    deps = [":SwiftProtobufPluginLibrary"],
)

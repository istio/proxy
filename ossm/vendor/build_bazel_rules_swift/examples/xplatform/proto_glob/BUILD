load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")
load("//swift:swift_binary.bzl", "swift_binary")

proto_library(
    name = "proto_glob",
    srcs = glob(["**/*.proto"]),
)

swift_proto_library(
    name = "proto_glob_swift",
    additional_compiler_info = {
        "FileNaming": "PathToUnderscores",
    },
    protos = [":proto_glob"],
)

swift_binary(
    name = "proto_glob_example",
    srcs = ["main.swift"],
    deps = [
        ":proto_glob_swift",
    ],
)

load(
    "@rules_proto//proto:defs.bzl",
    "proto_library",
)
load("//proto:swift_proto_library.bzl", "swift_proto_library")
load("//swift:swift_binary.bzl", "swift_binary")

proto_library(
    name = "message_1_proto",
    srcs = [
        "message_1.proto",
    ],
)

proto_library(
    name = "message_2_proto",
    srcs = [
        "message_2.proto",
    ],
    deps = [
        ":message_1_proto",
    ],
)

proto_library(
    name = "message_3_proto",
    srcs = [
        "message_3.proto",
    ],
    deps = [
        ":message_2_proto",
    ],
)

swift_proto_library(
    name = "messages_1_2_swift_proto",
    module_name = "Messages_1_2",
    protos = [
        ":message_1_proto",
        ":message_2_proto",
    ],
)

swift_proto_library(
    name = "messages_3_swift_proto",
    module_name = "Messages_3",
    protos = [
        ":message_3_proto",
    ],
    deps = [
        ":messages_1_2_swift_proto",
    ],
)

swift_binary(
    name = "proto_files_example",
    srcs = [
        "main.swift",
    ],
    deps = [
        ":messages_1_2_swift_proto",
        ":messages_3_swift_proto",
    ],
)

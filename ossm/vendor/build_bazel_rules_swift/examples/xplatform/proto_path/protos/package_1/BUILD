load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "message_1_proto",
    srcs = [
        "message_1.proto",
    ],
    strip_import_prefix = "/examples/xplatform/proto_path/protos",
    visibility = ["//visibility:public"],
)

swift_proto_library(
    name = "message_1_swift_proto",
    module_name = "Message_1",
    protos = [
        ":message_1_proto",
    ],
    visibility = ["//visibility:public"],
)

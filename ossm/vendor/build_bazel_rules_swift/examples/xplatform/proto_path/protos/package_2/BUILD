load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "message_2_proto",
    srcs = [
        "message_2.proto",
    ],
    strip_import_prefix = "/examples/xplatform/proto_path/protos",
    visibility = ["//visibility:public"],
    deps = [
        "//examples/xplatform/proto_path/protos/package_1:message_1_proto",
    ],
)

swift_proto_library(
    name = "message_2_swift_proto",
    module_name = "Message_2",
    protos = [
        ":message_2_proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//examples/xplatform/proto_path/protos/package_1:message_1_swift_proto",
    ],
)

# This package contains testing utility code
package(default_visibility = ["//visibility:public"])

licenses(["notice"])

proto_library(
    name = "test_message_proto",
    srcs = [
        "test_message.proto",
    ],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
        "@com_google_protobuf//:wrappers_proto",
    ],
)

cc_proto_library(
    name = "test_message_cc_proto",
    deps = [":test_message_proto"],
)

proto_library(
    name = "simple_test_message_proto",
    srcs = [
        "simple_test_message.proto",
    ],
)

proto_library(
    name = "test_extensions_proto",
    srcs = [
        "test_extensions.proto",
    ],
    deps = ["@com_google_protobuf//:wrappers_proto"],
)

cc_proto_library(
    name = "test_extensions_cc_proto",
    deps = [":test_extensions_proto"],
)

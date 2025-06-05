load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")

proto_library(
    name = "service_proto",
    srcs = [
        "service.proto",
        "service_messages.proto",
    ],
    deps = [
        "@com_google_protobuf//:any_proto",
    ],
)

swift_proto_library(
    name = "service_server_swift_proto",
    compilers = [
        "//proto/compilers:swift_proto",
        "//proto/compilers:swift_server_proto",
    ],
    module_name = "ServiceServer",
    protos = [":service_proto"],
    visibility = ["//visibility:public"],
)

swift_proto_library(
    name = "service_client_swift_proto",
    compilers = [
        "//proto/compilers:swift_proto",
        "//proto/compilers:swift_client_proto",
    ],
    module_name = "ServiceClient",
    protos = [":service_proto"],
    visibility = ["//visibility:public"],
)

swift_proto_library(
    name = "service_test_client_swift_proto",
    additional_compiler_deps = [
        "//examples/xplatform/grpc/service:service_client_swift_proto",
    ],
    additional_compiler_info = {
        "ExtraModuleImports": "ServiceClient",
    },
    compilers = [
        "//proto/compilers:swift_test_client_proto",
    ],
    module_name = "ServiceTestClient",
    protos = [":service_proto"],
    visibility = ["//visibility:public"],
)

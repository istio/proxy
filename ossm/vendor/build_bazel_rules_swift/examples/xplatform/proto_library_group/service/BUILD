load("@rules_proto//proto:defs.bzl", "proto_library")
load("//proto:swift_proto_library.bzl", "swift_proto_library")
load("//proto:swift_proto_library_group.bzl", "swift_proto_library_group")

proto_library(
    name = "service_proto",
    srcs = [
        "service.proto",
    ],
    deps = [
        "//examples/xplatform/proto_library_group/request:request_proto",
        "//examples/xplatform/proto_library_group/response:response_proto",
    ],
)

swift_proto_library_group(
    name = "proto_library_group_swift_proto",
    compiler = "//proto/compilers:swift_proto",
    proto = "//examples/xplatform/proto_library_group/response:response_proto",
)

swift_proto_library(
    name = "service_server_swift_proto",
    compilers = [
        "//proto/compilers:swift_server_proto",
    ],
    module_name = "ServiceServer",
    protos = [":service_proto"],
    visibility = ["//visibility:public"],
    deps = [
        "//examples/xplatform/proto_library_group/service:proto_library_group_swift_proto",
    ],
)

swift_proto_library(
    name = "service_client_swift_proto",
    compilers = [
        "//proto/compilers:swift_client_proto",
    ],
    module_name = "ServiceClient",
    protos = [":service_proto"],
    visibility = ["//visibility:public"],
    deps = [
        "//examples/xplatform/proto_library_group/service:proto_library_group_swift_proto",
    ],
)

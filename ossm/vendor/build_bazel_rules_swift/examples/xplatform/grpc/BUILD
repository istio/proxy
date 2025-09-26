# This example illustrates how to use `swift_grpc_library` along with
# `swift_proto_library` to build Swift binaries for a gRPC server and
# client.
#
# To explore this example:
#
# 1.  Build both the `:echo_server` and `:echo_client` targets in this package.
# 2.  Run the `echo_server` binary in the background of a terminal. It should
#     print a message indicating that the service has started.
# 3.  Run the `echo_client` binary in the same terminal. It will send a request
#     to this service and then print the response that it received.

load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_test.bzl", "swift_test")

licenses(["notice"])

swift_binary(
    name = "echo_server",
    srcs = ["server_main.swift"],
    deps = [
        "//examples/xplatform/grpc/service:service_server_swift_proto",
    ],
)

swift_binary(
    name = "echo_client",
    srcs = ["client_main.swift"],
    deps = [
        "//examples/xplatform/grpc/service:service_client_swift_proto",
    ],
)

swift_test(
    name = "echo_client_unit_test",
    srcs = [
        "client_unit_test.swift",
    ],
    deps = [
        "//examples/xplatform/grpc/service:service_client_swift_proto",
        "//examples/xplatform/grpc/service:service_server_swift_proto",
    ],
)

swift_test(
    name = "echo_test_client_unit_test",
    srcs = [
        "test_client_unit_test.swift",
    ],
    deps = [
        "//examples/xplatform/grpc/service:service_client_swift_proto",
        "//examples/xplatform/grpc/service:service_test_client_swift_proto",
    ],
)

licenses(["notice"])

load("@protobuf_bzl//:protobuf.bzl", "cc_proto_library")

exports_files(glob(["google/**"]))

cc_proto_library(
    name = "cc_gogoproto",
    srcs = [
        "gogoproto/gogo.proto",
    ],
    include = ".",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_wkt_protos",
    ],
)
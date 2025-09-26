load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "api_lib",
    hdrs = [
        "proxy_wasm_api.h",
        "proxy_wasm_externs.h",
    ],
    copts = ["-std=c++17"],
    deps = [
        ":common_lib",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_library(
    name = "common_lib",
    hdrs = [
        "proxy_wasm_common.h",
        "proxy_wasm_enums.h",
    ],
    copts = ["-std=c++17"],
)

cc_library(
    name = "proxy_wasm_intrinsics",
    srcs = [
        "proxy_wasm_intrinsics.cc",
    ],
    hdrs = [
        "proxy_wasm_api.h",
        "proxy_wasm_common.h",
        "proxy_wasm_enums.h",
        "proxy_wasm_externs.h",
        "proxy_wasm_intrinsics.h",
    ],
    copts = ["-std=c++17"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "proxy_wasm_intrinsics_cc_proto",
    deps = [":proxy_wasm_intrinsics_proto"],
)

proto_library(
    name = "proxy_wasm_intrinsics_proto",
    srcs = ["proxy_wasm_intrinsics.proto"],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

# include lite protobuf support
cc_library(
    name = "proxy_wasm_intrinsics_lite",
    hdrs = ["proxy_wasm_intrinsics_lite.h"],
    copts = ["-std=c++17"],
    defines = ["PROXY_WASM_PROTOBUF_LITE"],
    visibility = ["//visibility:public"],
    deps = [
        ":proxy_wasm_intrinsics",
        ":proxy_wasm_intrinsics_lite_cc_proto",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

# include full protobuf support
cc_library(
    name = "proxy_wasm_intrinsics_full",
    hdrs = ["proxy_wasm_intrinsics_full.h"],
    copts = ["-std=c++17"],
    defines = ["PROXY_WASM_PROTOBUF_FULL"],
    visibility = ["//visibility:public"],
    deps = [
        ":proxy_wasm_intrinsics",
        ":proxy_wasm_intrinsics_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_proto_library(
    name = "proxy_wasm_intrinsics_lite_cc_proto",
    deps = [":proxy_wasm_intrinsics_lite_proto"],
)

proto_library(
    name = "proxy_wasm_intrinsics_lite_proto",
    srcs = [
        "proxy_wasm_intrinsics_lite.proto",
        "struct_lite.proto",
    ],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:struct_proto",
    ],
)

filegroup(
    name = "proxy_wasm_intrinsics_js",
    srcs = [
        "proxy_wasm_intrinsics.js",
    ],
    visibility = ["//visibility:public"],
)

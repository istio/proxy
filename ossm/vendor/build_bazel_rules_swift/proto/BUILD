load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//visibility:public"])

exports_files(["copy_swift_sources.sh.tpl"])

bzl_library(
    name = "swift_proto_utils",
    srcs = ["swift_proto_utils.bzl"],
    deps = [
        "//swift/internal:output_groups",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "swift_proto_common",
    srcs = ["swift_proto_common.bzl"],
    deps = [":swift_proto_utils"],
)

bzl_library(
    name = "swift_proto_compiler",
    srcs = ["swift_proto_compiler.bzl"],
    deps = [
        ":swift_proto_utils",
        "@bazel_skylib//lib:paths",
    ],
)

bzl_library(
    name = "swift_proto_library",
    srcs = ["swift_proto_library.bzl"],
    deps = [
        ":swift_proto_utils",
        "//swift:module_name",
        "//swift:swift_clang_module_aspect",
        "//swift:swift_common",
        "//swift/internal:attrs",
        "//swift/internal:toolchain_utils",
        "//swift/internal:utils",
        "@bazel_skylib//lib:dicts",
        "@rules_proto//proto:defs",
    ],
)

label_setting(
    name = "_swift_proto_compiler",
    build_setting_default = "//proto/compilers:swift_proto",
    visibility = ["//proto:__pkg__"],
)

bzl_library(
    name = "swift_proto_library_group",
    srcs = ["swift_proto_library_group.bzl"],
    deps = [
        ":swift_proto_utils",
        "//swift:module_name",
        "//swift:providers",
        "//swift:swift_common",
        "//swift/internal:toolchain_utils",
        "//swift/internal:utils",
        "@bazel_skylib//lib:dicts",
        "@rules_proto//proto:defs",
    ],
)

bzl_library(
    name = "proto",
    srcs = ["proto.bzl"],
    deps = [
        ":swift_proto_common",
        ":swift_proto_compiler",
        ":swift_proto_library",
        ":swift_proto_library_group",
        "//swift:providers.bzl",
    ],
)

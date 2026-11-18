load("@bazel_lib//:bzl_library.bzl", "bzl_library")
load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

bool_flag(
    name = "tar_compute_unused_inputs",
    # TODO(1.0): change default to True, but without a breaking change to bazel-lib
    build_setting_default = False,
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "mtree",
    srcs = ["mtree.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        "//tar/private:tar",
        "@bazel_skylib//lib:partial",
        "@bazel_skylib//lib:types",
    ],
)

bzl_library(
    name = "tar",
    srcs = ["tar.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        ":mtree",
        "//tar/private:tar",
        "@bazel_lib//lib:expand_template",
        "@bazel_lib//lib:utils",
        "@bazel_skylib//lib:partial",
        "@bazel_skylib//lib:types",
    ],
)

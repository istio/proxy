load("//:bzl_library.bzl", "bzl_library")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

bzl_library(
    name = "analysis_test",
    srcs = ["analysis_test.bzl"],
)

bzl_library(
    name = "build_test",
    srcs = ["build_test.bzl"],
    deps = ["//lib:new_sets"],
)

bzl_library(
    name = "copy_file",
    srcs = ["copy_file.bzl"],
    deps = ["//rules/private:copy_file_private"],
)

bzl_library(
    name = "copy_directory",
    srcs = ["copy_directory.bzl"],
    deps = ["//rules/private:copy_directory_private"],
)

bzl_library(
    name = "write_file",
    srcs = ["write_file.bzl"],
    deps = ["//rules/private:write_file_private"],
)

bzl_library(
    name = "diff_test",
    srcs = ["diff_test.bzl"],
    deps = ["//lib:shell"],
)

bzl_library(
    name = "expand_template",
    srcs = ["expand_template.bzl"],
)

bzl_library(
    name = "native_binary",
    srcs = ["native_binary.bzl"],
)

bzl_library(
    name = "run_binary",
    srcs = ["run_binary.bzl"],
    deps = ["//lib:dicts"],
)

bzl_library(
    name = "common_settings",
    srcs = ["common_settings.bzl"],
)

filegroup(
    name = "test_deps",
    testonly = True,
    srcs = [
        "BUILD",
    ] + glob(["*.bzl"]),
)

# The files needed for distribution
filegroup(
    name = "distribution",
    srcs = [
        "BUILD",
    ] + glob(["*.bzl"]),
    visibility = [
        "//:__pkg__",
    ],
)

# export bzl files for the documentation
exports_files(
    glob(["*.bzl"]),
    visibility = ["//:__subpackages__"],
)

bzl_library(
    name = "select_file",
    srcs = ["select_file.bzl"],
)

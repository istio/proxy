load("//:bzl_library.bzl", "bzl_library")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

# export bzl files for the documentation
exports_files(
    glob(["*.bzl"]),
    visibility = ["//:__subpackages__"],
)

bzl_library(
    name = "collections",
    srcs = ["collections.bzl"],
)

bzl_library(
    name = "dicts",
    srcs = ["dicts.bzl"],
)

bzl_library(
    name = "modules",
    srcs = ["modules.bzl"],
)

bzl_library(
    name = "partial",
    srcs = ["partial.bzl"],
)

bzl_library(
    name = "paths",
    srcs = ["paths.bzl"],
)

bzl_library(
    name = "selects",
    srcs = ["selects.bzl"],
)

bzl_library(
    name = "sets",
    srcs = ["sets.bzl"],
    deps = [
        ":new_sets",
    ],
)

bzl_library(
    name = "new_sets",
    srcs = ["new_sets.bzl"],
    deps = [
        ":dicts",
    ],
)

bzl_library(
    name = "shell",
    srcs = ["shell.bzl"],
)

bzl_library(
    name = "structs",
    srcs = ["structs.bzl"],
)

bzl_library(
    name = "subpackages",
    srcs = ["subpackages.bzl"],
)

bzl_library(
    name = "types",
    srcs = ["types.bzl"],
)

bzl_library(
    name = "unittest",
    srcs = ["unittest.bzl"],
    deps = [
        ":new_sets",
        ":partial",
        ":sets",
        ":types",
    ],
)

bzl_library(
    name = "versions",
    srcs = ["versions.bzl"],
)

filegroup(
    name = "test_deps",
    testonly = True,
    srcs = ["BUILD"] + glob(["*.bzl"]),
)

# The files needed for distribution
filegroup(
    name = "distribution",
    srcs = glob(["*"]),
    visibility = [
        "//:__pkg__",
        "//distribution:__pkg__",
    ],
)

bzl_library(
    name = "old_sets",
    srcs = ["old_sets.bzl"],
)

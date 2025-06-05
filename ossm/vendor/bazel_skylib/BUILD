load("@rules_license//rules:license.bzl", "license")
load("//:bzl_library.bzl", "bzl_library")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

license(
    name = "license",
    package_name = "bazelbuild/bazel_skylib",
    license_kinds = ["@rules_license//licenses/spdx:Apache-2.0"],
)

licenses(["notice"])

# buildifier: disable=skylark-comment
# gazelle:exclude skylark_library.bzl

exports_files([
    "LICENSE",
    "MODULE.bazel",
    "WORKSPACE",
])

filegroup(
    name = "test_deps",
    testonly = True,
    srcs = [
        "BUILD",
        "//lib:test_deps",
        "//rules:test_deps",
        "//toolchains/unittest:test_deps",
    ] + glob(["*.bzl"]),
)

bzl_library(
    name = "lib",
    srcs = ["lib.bzl"],
    deprecation = (
        "lib.bzl will go away in the future, please directly depend on the" +
        " module(s) needed as it is more efficient."
    ),
    deps = [
        "//lib:collections",
        "//lib:dicts",
        "//lib:new_sets",
        "//lib:partial",
        "//lib:paths",
        "//lib:selects",
        "//lib:sets",
        "//lib:shell",
        "//lib:structs",
        "//lib:types",
        "//lib:unittest",
        "//lib:versions",
    ],
)

bzl_library(
    name = "bzl_library",
    srcs = ["bzl_library.bzl"],
)

bzl_library(
    name = "version",
    srcs = ["version.bzl"],
)

bzl_library(
    name = "workspace",
    srcs = ["workspace.bzl"],
)

# The files needed for distribution.
# TODO(aiuto): We should strip this from the release, but there is no
# capability now to generate BUILD.foo from BUILD and have it appear in the
# tarball as BUILD.
filegroup(
    name = "distribution",
    srcs = [
        "BUILD",
        "CODEOWNERS",
        "CONTRIBUTORS",
        "LICENSE",
        "WORKSPACE.bzlmod",
        "//lib:distribution",
        "//rules:distribution",
        "//rules/directory:distribution",
        "//rules/directory/private:distribution",
        "//rules/private:distribution",
        "//toolchains/unittest:distribution",
    ] + glob(["*.bzl"]),
)

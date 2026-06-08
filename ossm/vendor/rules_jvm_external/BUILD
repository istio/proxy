load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@package_metadata//licenses/rules:license.bzl", "license")
load("@package_metadata//rules:package_metadata.bzl", "package_metadata")

exports_files([
    "defs.bzl",
    "extensions.bzl",
    "specs.bzl",
    "maven_install.json",
])

licenses(["notice"])  # Apache 2.0

package_metadata(
    name = "package_metadata",
    attributes = [
        ":license",
    ],
    purl = "pkg:bazel/{}@{}".format(
        module_name(),
        module_version(),
    ) if module_version() else "pkg:bazel/{}".format(module_name()),
    visibility = ["//visibility:public"],
)

license(
    name = "license",
    kind = "@package_metadata//licenses/spdx:Apache-2.0",
    text = "LICENSE",
)

bzl_library(
    name = "implementation",
    srcs = [
        ":defs.bzl",
        ":specs.bzl",
        "@bazel_features//:bzl_files",
        "@package_metadata//:srcs",
        "@rules_license//:docs_deps",
    ],
    visibility = [
        # This library is only visible to allow others who depend on
        # `rules_jvm_external` to be able to document their code using
        # stardoc.
        "//visibility:public",
    ],
    deps = [
        "//private:implementation",
        "//private/extensions:implementation",
        "//private/lib:implementation",
        "//private/rules:implementation",
        "//settings:implementation",
        "@bazel_features//:bzl_files",
        "@bazel_skylib//lib:new_sets",
        "@bazel_tools//tools:bzl_srcs",
        "@rules_java//java:rules",
    ],
)

alias(
    name = "mirror_coursier",
    actual = "//scripts:mirror_coursier",
)

alias(
    name = "generate_api_reference",
    actual = "//scripts:generate_api_reference",
)

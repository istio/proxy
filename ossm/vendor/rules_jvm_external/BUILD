load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

exports_files([
    "defs.bzl",
    "specs.bzl",
])

licenses(["notice"])  # Apache 2.0

bzl_library(
    name = "implementation",
    srcs = [
        ":defs.bzl",
        ":specs.bzl",
        "@bazel_features//:bzl_files",
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
        "//private/lib:implementation",
        "//private/rules:implementation",
        "//settings:implementation",
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

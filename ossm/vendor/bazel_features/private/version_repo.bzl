"""Contains the internal repository rule version_repo."""

def _version_repo_impl(rctx):
    rctx.file(
        "BUILD.bazel",
        """
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

exports_files(["version.bzl"])

bzl_library(
    name = "version",
    srcs = ["version.bzl"],
    visibility = ["//visibility:public"],
)
""",
    )
    rctx.file("version.bzl", "version = '" + native.bazel_version + "'")

version_repo = repository_rule(
    _version_repo_impl,
    # Force reruns on server restarts to keep native.bazel_version up-to-date.
    local = True,
)

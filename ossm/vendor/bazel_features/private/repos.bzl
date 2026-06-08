"""Contains the macro bazel_features_repos to install internal repositories."""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load(":globals.bzl", "GLOBALS")
load(":globals_repo.bzl", "globals_repo")
load(":version_repo.bzl", "version_repo")

def bazel_features_repos():
    maybe(
        version_repo,
        name = "bazel_features_version",
    )
    maybe(
        globals_repo,
        name = "bazel_features_globals",
        globals = GLOBALS,
    )

"""Internal module extensions."""

load("@bazel_skylib//lib:modules.bzl", "modules")
load("//private:repos.bzl", "bazel_features_repos")

version_extension = modules.as_extension(bazel_features_repos)

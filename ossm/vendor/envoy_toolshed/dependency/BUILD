load("//dependency:macros.bzl", "updater")

exports_files([
    "bazel-update.sh",
    "macros.bzl",
])

updater(
    name = "update",
    dependencies = "//:dependency_versions",
    version_file = "//:versions.bzl",
)

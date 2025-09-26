load("//dependency:macros.bzl", "updater")

exports_files([
    "bazel-update.sh",
])

updater(
    name = "update",
    dependencies = "//:dependency_versions",
    version_file = "//:versions.bzl",
)

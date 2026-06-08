load("@aspect_bazel_lib//lib:jq.bzl", "jq")
load("//:macros.bzl", "json_data")
load("//:versions.bzl", "VERSIONS")
load("//version:utils.bzl", "module_versions")

exports_files([
    "jq.sh",
    "versions.bzl",
    "macros.bzl",
    "packages.bzl",
])

json_data(
    name = "deps",
    data = VERSIONS,
)

jq(
    name = "dependency_versions",
    srcs = [":deps"],
    out = "dependency_shas.json",
    filter = """
    with_entries(select(.value | objects and .type == "github_archive") | .value |= {repo, sha256, url, version})
    """,
    visibility = ["//visibility:public"],
)

# Example usage of module_versions to extract bzlmod dependency versions
module_versions(
    name = "module_dependency_versions",
    visibility = ["//visibility:public"],
)

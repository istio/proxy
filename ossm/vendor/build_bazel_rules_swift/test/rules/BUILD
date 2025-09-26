load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_visibility = ["//test:__subpackages__"])

bzl_library(
    name = "swift_library_artifact_collector",
    srcs = ["swift_library_artifact_collector.bzl"],
    deps = ["//swift:providers"],
)

exports_files(["swift_shell_runner.sh.template"])

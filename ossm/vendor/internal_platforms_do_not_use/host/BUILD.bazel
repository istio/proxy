# Host platform detection

load("@host_platform//:constraints.bzl", "HOST_CONSTRAINTS")

package(default_visibility = ["//visibility:public"])

exports_files(["constraints.bzl", "extension.bzl"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

platform(
    name = "host",
    constraint_values = HOST_CONSTRAINTS,
)

# The following filegroup targets are essentially bzl_library targets.
# We don't directly use bzl_library to avoid a dependency on bazel-skylib.
filegroup(
    name = "constraints_lib",
    srcs = [
        "constraints.bzl",
        "@host_platform//:constraints.bzl",
    ],
)

filegroup(
    name = "extension_lib",
    srcs = [
        "extension.bzl",
    ],
)

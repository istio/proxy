load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("//settings:stamp_manifest.bzl", "stamp_manifest")

package(
    default_visibility = ["//visibility:public"],
)

bzl_library(
    name = "implementation",
    srcs = glob(["*.bzl"]),
    visibility = ["//:__subpackages__"],
)

stamp_manifest(
    name = "stamp_manifest",
    build_setting_default = True,
)

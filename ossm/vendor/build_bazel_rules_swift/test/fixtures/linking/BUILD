load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_library.bzl", "swift_library")
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")
load(":fake_framework.bzl", "fake_framework")

package(
    default_visibility = ["//test:__subpackages__"],
)

fake_framework(
    name = "framework",
    tags = FIXTURE_TAGS,
)

swift_binary(
    name = "bin",
    srcs = ["main.swift"],
    tags = FIXTURE_TAGS,
    deps = [":framework"],
)

swift_library(
    name = "lib",
    srcs = ["lib.swift"],
    tags = FIXTURE_TAGS,
)

cc_binary(
    name = "cc_bin",
    srcs = ["main.cc"],
    tags = FIXTURE_TAGS,
    deps = [":lib"],
)

bzl_library(
    name = "fake_framework",
    srcs = ["fake_framework.bzl"],
    deps = ["@bazel_tools//tools/cpp:toolchain_utils.bzl"],
)

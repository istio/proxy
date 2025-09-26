load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_library_group.bzl", "swift_library_group")
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

swift_library(
    name = "first",
    srcs = ["first.swift"],
    module_name = "first",
    tags = FIXTURE_TAGS,
)

swift_library_group(
    name = "library_group",
    tags = FIXTURE_TAGS,
    deps = ["first"],
)

swift_library(
    name = "second",
    srcs = ["second.swift"],
    module_name = "second",
    tags = FIXTURE_TAGS,
    deps = ["library_group"],
)

load(
    "//swift:swift_library.bzl",
    "swift_library",
)
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

###############################################################################
# Fixtures for testing swift_libraries propagate through objc_libraries

swift_library(
    name = "lower",
    srcs = ["Empty.swift"],
    tags = FIXTURE_TAGS,
)

cc_library(
    name = "middle",
    hdrs = ["Empty.h"],
    tags = FIXTURE_TAGS,
    deps = [":lower"],
)

swift_library(
    name = "upper",
    srcs = ["Empty.swift"],
    tags = FIXTURE_TAGS,
    deps = [":middle"],
)

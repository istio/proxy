load("//swift:swift_library.bzl", "swift_library")
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

swift_library(
    name = "multiple_files",
    srcs = [
        "Empty.swift",
        "Empty2.swift",
    ],
    generates_header = False,
    tags = FIXTURE_TAGS,
)

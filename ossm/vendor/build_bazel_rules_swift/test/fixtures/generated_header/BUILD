load(
    "//swift:swift_library.bzl",
    "swift_library",
)
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

swift_library(
    name = "no_header",
    srcs = ["Empty.swift"],
    generates_header = False,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "auto_header",
    srcs = ["Empty.swift"],
    generates_header = True,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "explicit_header",
    srcs = ["Empty.swift"],
    generated_header_name = "SomeOtherName.h",
    generates_header = True,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "invalid_extension",
    srcs = ["Empty.swift"],
    generated_header_name = "Invalid.extension",
    generates_header = True,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "valid_path_separator",
    srcs = ["Empty.swift"],
    generated_header_name = "Valid/Separator.h",
    generates_header = True,
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "invalid_attribute_combination",
    srcs = ["Empty.swift"],
    generated_header_name = "SomeOtherName.h",
    generates_header = False,
    tags = FIXTURE_TAGS,
)

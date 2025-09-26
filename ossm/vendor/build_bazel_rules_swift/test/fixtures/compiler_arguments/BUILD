load("//swift:swift_binary.bzl", "swift_binary")
load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_test.bzl", "swift_test")
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

###############################################################################
# Fixtures for testing serializing debug options.

swift_library(
    name = "no_package_name",
    srcs = ["empty.swift"],
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "lib_package_name",
    package_name = "lib",
    srcs = ["empty.swift"],
    tags = FIXTURE_TAGS,
)

swift_binary(
    name = "bin_package_name",
    package_name = "bin",
    srcs = ["empty.swift"],
    tags = FIXTURE_TAGS,
)

swift_test(
    name = "test_package_name",
    package_name = "test",
    srcs = ["empty.swift"],
    discover_tests = False,
    tags = FIXTURE_TAGS,
)

swift_binary(
    name = "bin",
    srcs = ["empty.swift"],
    tags = FIXTURE_TAGS,
)

load("//swift:swift_test.bzl", "swift_test")
load("//test/fixtures:common.bzl", "FIXTURE_TAGS")

package(
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

###############################################################################
# Fixtures for testing xctest_runner outputs

swift_test(
    name = "PassingUnitTests",
    srcs = ["PassingUnitTests.swift"],
    tags = FIXTURE_TAGS,
)

swift_test(
    name = "FailingUnitTests",
    srcs = ["FailingUnitTests.swift"],
    tags = FIXTURE_TAGS,
)

swift_test(
    name = "EmptyUnitTests",
    tags = FIXTURE_TAGS,
)

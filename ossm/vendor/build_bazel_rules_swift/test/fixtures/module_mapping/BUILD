load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_module_mapping.bzl", "swift_module_mapping")
load("//swift:swift_module_mapping_test.bzl", "swift_module_mapping_test")
load(
    "//test/fixtures:common.bzl",
    "FIXTURE_TAGS",
)
load(":apply_mapping.bzl", "apply_mapping")

package(
    default_testonly = True,
    default_visibility = ["//test:__subpackages__"],
)

licenses(["notice"])

swift_library(
    name = "Common",
    srcs = ["Common.swift"],
    module_name = "Common",
    tags = FIXTURE_TAGS,
)

swift_library(
    name = "MySDK",
    srcs = ["MySDK.swift"],
    module_name = "MySDK",
    tags = FIXTURE_TAGS,
    deps = [":Common"],
)

swift_module_mapping(
    name = "MySDK_module_mapping",
    # This must not be testonly because it is used by the toolchain through the
    # `:module_mapping` label flag.
    testonly = False,
    aliases = {
        "Common": "MySDKInternal_Common",
    },
    tags = FIXTURE_TAGS,
)

# This is the target that will be tested in `module_mapping.bzl`, to force the
# `MySDK` target to build in a configuration that sets the flag.
apply_mapping(
    name = "MySDK_with_mapping",
    mapping = ":MySDK_module_mapping",
    tags = FIXTURE_TAGS,
    target = ":MySDK",
)

swift_library(
    name = "ExistingLibrary",
    srcs = ["Empty.swift"],
    module_name = "ExistingLibrary",
    tags = FIXTURE_TAGS,
    deps = [":NewDependency"],
)

swift_library(
    name = "NewDependency",
    srcs = ["Empty.swift"],
    module_name = "NewDependency",
    tags = FIXTURE_TAGS,
)

swift_module_mapping(
    name = "ExistingLibrary_module_mapping_incomplete",
    aliases = {
        "ExistingLibrary": "MySDKInternal_ExistingLibrary",
    },
    tags = FIXTURE_TAGS,
)

swift_module_mapping(
    name = "ExistingLibrary_module_mapping_complete",
    aliases = {
        "ExistingLibrary": "MySDKInternal_ExistingLibrary",
        "NewDependency": "MySDKInternal_NewDependency",
    },
    tags = FIXTURE_TAGS,
)

# We can't write a test that verifies that this *test fails at execution time*.
# It's been marked manual so we can run it directly to verify its behavior.
# Other tests that do work automatically are in `module_mapping_tests.bzl`.
swift_module_mapping_test(
    name = "ExistingLibrary_module_mapping_incomplete_test",
    mapping = ":ExistingLibrary_module_mapping_incomplete",
    tags = FIXTURE_TAGS,
    deps = [":ExistingLibrary"],
)

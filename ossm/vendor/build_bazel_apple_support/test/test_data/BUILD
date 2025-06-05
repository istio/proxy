load("//rules:universal_binary.bzl", "universal_binary")
load("//test:starlark_apple_binary.bzl", "starlark_apple_binary")
load("//test:starlark_apple_static_library.bzl", "starlark_apple_static_library")

package(
    default_testonly = 1,
    default_visibility = ["//test:__subpackages__"],
)

TARGETS_UNDER_TEST_TAGS = [
    "manual",
    "notap",
]

cc_binary(
    name = "cc_test_binary",
    srcs = ["main.cc"],
    tags = TARGETS_UNDER_TEST_TAGS,
)

cc_library(
    name = "cc_main",
    srcs = ["main.cc"],
    tags = TARGETS_UNDER_TEST_TAGS,
)

universal_binary(
    name = "multi_arch_cc_binary",
    binary = ":cc_test_binary",
    tags = TARGETS_UNDER_TEST_TAGS,
)

starlark_apple_binary(
    name = "macos_binary",
    minimum_os_version = "13.0",
    platform_type = "macos",
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":cc_main"],
)

starlark_apple_binary(
    name = "macos_binary_with_spaces",
    minimum_os_version = "13.0",
    platform_type = "macos",
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":objc_lib_with_spaces"],
)

cc_library(
    name = "cc_lib",
    srcs = ["cc_lib.cc"],
    tags = TARGETS_UNDER_TEST_TAGS,
)

objc_library(
    name = "objc_lib",
    srcs = ["objc_lib.m"],
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = ["cc_lib"],
)

objc_library(
    name = "objc_lib_with_spaces",
    srcs = ["objc lib with spaces.m"],
    tags = TARGETS_UNDER_TEST_TAGS,
)

starlark_apple_static_library(
    name = "static_lib",
    minimum_os_version = "15.0",
    platform_type = "ios",
    deps = [":objc_lib"],
)

objc_library(
    name = "objc_main",
    srcs = ["main.m"],
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = ["objc_lib"],
)

objc_library(
    name = "objc_lib_with_unused_symbol",
    srcs = ["objc_lib_with_unused_symbol.m"],
    tags = TARGETS_UNDER_TEST_TAGS,
)

starlark_apple_binary(
    name = "ios_app_with_unused_symbol",
    minimum_os_version = "13.0",
    platform_type = "ios",
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":objc_lib_with_unused_symbol"],
)

starlark_apple_binary(
    name = "visionos_binary",
    minimum_os_version = "1.0",
    platform_type = "visionos",
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":objc_main"],
)

objc_library(
    name = "watch_main",
    srcs = ["watch_main.m"],
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":cc_lib"],
)

starlark_apple_binary(
    name = "watch_binary",
    minimum_os_version = "8.0",
    platform_type = "watchos",
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":watch_main"],
)

objc_library(
    name = "ios_main",
    srcs = ["ios_main.m"],
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":cc_lib"],
)

starlark_apple_binary(
    name = "ios_binary",
    minimum_os_version = "15.0",
    platform_type = "ios",
    tags = TARGETS_UNDER_TEST_TAGS,
    deps = [":ios_main"],
)

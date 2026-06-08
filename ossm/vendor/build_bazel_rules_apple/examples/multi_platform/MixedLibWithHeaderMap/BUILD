load(
    "@build_bazel_rules_swift//swift:swift.bzl",
    "swift_library",
)
load("//apple:apple.bzl", "experimental_mixed_language_library")
load("//apple:ios.bzl", "ios_unit_test")

filegroup(
    name = "MixedAnswer.public_headers.h",
    srcs = [
        "MixedAnswer.h",
    ],
)

experimental_mixed_language_library(
    name = "MixedAnswer",
    srcs = [
        "MixedAnswer.m",
        "MixedAnswer.swift",
    ],
    hdrs = [
        ":MixedAnswer.public_headers.h",
    ],
    enable_header_map = True,
    enable_modules = True,
)

swift_library(
    name = "SwiftLibDependingOnMixedLibWithHeaderMap",
    srcs = [
        "SwiftLibDependingOnMixedLibWithHeaderMap.swift",
    ],
    deps = [":MixedAnswer"],
)

objc_library(
    name = "ObjcLibDependingOnMixedLibWithHeaderMap",
    srcs = [
        "ObjcLibDependingOnMixedLibWithHeaderMap.m",
    ],
    hdrs = [
        "ObjcLibDependingOnMixedLibWithHeaderMap.h",
    ],
    copts = ["-I."],  # Enable importing headers via angle bracket import from the current directory.
    enable_modules = True,
    deps = [":MixedAnswer"],
)

experimental_mixed_language_library(
    name = "MixedTestsLib",
    testonly = True,
    srcs = [
        "MixedTests.m",
        "MixedTests.swift",
    ],
)

ios_unit_test(
    name = "MixedTests",
    minimum_os_version = "9.0",
    deps = [":MixedTestsLib"],
)

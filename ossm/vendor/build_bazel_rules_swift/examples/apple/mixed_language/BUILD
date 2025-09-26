load("//mixed_language:mixed_language_library.bzl", "mixed_language_library")
load("//swift:swift_library.bzl", "swift_library")
load("//swift:swift_test.bzl", "swift_test")

mixed_language_library(
    name = "MixedAnswer",
    hdrs = ["MixedAnswer.h"],
    clang_srcs = [
        "MixedAnswer.m",
        "MixedAnswerPrivate.m",
        "MixedAnswerPrivate.h",
    ],
    enable_modules = True,
    module_name = "MixedAnswer",
    swift_srcs = [
        "MixedAnswer.swift",
    ],
    target_compatible_with = ["@platforms//os:macos"],
)

swift_library(
    name = "SwiftLibDependingOnMixedLib",
    srcs = [
        "SwiftLibDependingOnMixedLib.swift",
    ],
    module_name = "SwiftLibDependingOnMixedLib",
    deps = [":MixedAnswer"],
)

mixed_language_library(
    name = "MixedTestsLib",
    testonly = True,
    clang_srcs = [
        "MixedTests.m",
    ],
    swift_srcs = [
        "MixedTests.swift",
    ],
    target_compatible_with = ["@platforms//os:macos"],
    deps = [
        ":SwiftLibDependingOnMixedLib",
    ],
)

swift_test(
    name = "MixedTests",
    deps = [":MixedTestsLib"],
)

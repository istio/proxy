load("//swift:swift_test.bzl", "swift_test")

licenses(["notice"])

swift_test(
    name = "xctest",
    srcs = [
        "SimpleTests.swift",
    ],
    env = {
        "XCTEST_ENV_VAR": "TRUE",
        "XCTEST_BINDIR_ENV_VAR": "$(BINDIR)",
    },
)

swift_test(
    name = "xctest_tsan",
    srcs = [
        "SimpleTests.swift",
    ],
    env = {
        "XCTEST_ENV_VAR": "TRUE",
        "XCTEST_BINDIR_ENV_VAR": "$(BINDIR)",
    },
    features = ["tsan"],
    # TODO: Re-enable on Linux once Swift race is figured out (or maybe Swift is upgraded)
    target_compatible_with = ["@platforms//os:macos"],
)

swift_test(
    name = "xctest_asan",
    srcs = [
        "SimpleTests.swift",
    ],
    env = {
        "XCTEST_ENV_VAR": "TRUE",
        "XCTEST_BINDIR_ENV_VAR": "$(BINDIR)",
    },
    features = ["asan"],
    # TODO: Re-enable on Linux once Swift memory leak is figured out (or maybe Swift is upgraded)
    target_compatible_with = ["@platforms//os:macos"],
)

swift_test(
    name = "xctest_ubsan",
    srcs = [
        "SimpleTests.swift",
    ],
    env = {
        "XCTEST_ENV_VAR": "TRUE",
        "XCTEST_BINDIR_ENV_VAR": "$(BINDIR)",
    },
    features = ["ubsan"],
    # TODO: Re-enable on Linux if Swift ever supports ubsan on linux
    target_compatible_with = ["@platforms//os:macos"],
)

swift_test(
    name = "xctest_tsan_and_ubsan",
    srcs = [
        "SimpleTests.swift",
    ],
    env = {
        "XCTEST_ENV_VAR": "TRUE",
        "XCTEST_BINDIR_ENV_VAR": "$(BINDIR)",
    },
    features = [
        "tsan",
        "ubsan",
    ],
    # TODO: Re-enable on Linux if Swift ever supports ubsan on linux
    target_compatible_with = ["@platforms//os:macos"],
)

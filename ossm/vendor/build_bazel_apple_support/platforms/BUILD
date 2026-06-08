load(
    "//configs:platforms.bzl",
    "APPLE_PLATFORMS_CONSTRAINTS",
    "CPU_TO_DEFAULT_PLATFORM_NAME",
)

package(default_visibility = ["//visibility:public"])

[
    platform(
        name = cpu,
        constraint_values = constraints,
    )
    for cpu, constraints in APPLE_PLATFORMS_CONSTRAINTS.items()
]

alias(
    name = "macos_x86_64",
    actual = "darwin_x86_64",
)

alias(
    name = "macos_arm64",
    actual = "darwin_arm64",
)

alias(
    name = "macos_arm64e",
    actual = "darwin_arm64e",
)

[
    label_flag(
        name = platform + "_platform",
        build_setting_default = platform,
    )
    for platform in CPU_TO_DEFAULT_PLATFORM_NAME.values()
]

# Consumed by bazel tests.
filegroup(
    name = "for_bazel_tests",
    testonly = True,
    srcs = glob(["**"]),
    visibility = ["//:__pkg__"],
)

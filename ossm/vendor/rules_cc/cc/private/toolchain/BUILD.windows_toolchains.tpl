load("@platforms//host:constraints.bzl", "HOST_CONSTRAINTS")

toolchain(
    name = "cc-toolchain-arm64_windows",
    exec_compatible_with = HOST_CONSTRAINTS,
    target_compatible_with = [
        "@platforms//cpu:arm64",
        "@platforms//os:windows",
    ],
    toolchain = "@local_config_cc//:cc-compiler-arm64_windows",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

toolchain(
    name = "cc-toolchain-x64_windows",
    exec_compatible_with = HOST_CONSTRAINTS,
    target_compatible_with = [
        "@platforms//cpu:x86_64",
        "@platforms//os:windows",
    ],
    toolchain = "@local_config_cc//:cc-compiler-x64_windows",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

toolchain(
    name = "cc-toolchain-armeabi-v7a",
    exec_compatible_with = HOST_CONSTRAINTS,
    target_compatible_with = [
        "@platforms//cpu:armv7",
        "@platforms//os:android",
    ],
    toolchain = "@local_config_cc//:cc-compiler-armeabi-v7a",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

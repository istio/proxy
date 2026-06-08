load(":default_test_runner.bzl", "compat_toolchain", "default_test_runner")

default_test_runner(name = "default_test_runner")

# If use_target_platform_constraints is supported by this version of
# Bazel, this toolchain forces the exec platform to match all constraints
# of the target platform. Otherwise the toolchain matches all platforms.
compat_toolchain(
    name = "default_test_runner_toolchain",
    toolchain = ":default_test_runner",
    toolchain_type = "@bazel_tools//tools/cpp:test_runner_toolchain_type",
    use_target_platform_constraints = True,
)

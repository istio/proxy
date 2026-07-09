platform(
    name = "host",
    parents = ["@local_config_platform//:host"],
    constraint_values = [
        "@platforms//cpu:ppc64le",
    ],
)

toolchain(
    name = "jdk_runtime",
    toolchain = "@local_jdk//:jdk",
    toolchain_type = "@bazel_tools//tools/jdk:runtime_toolchain_type",
    exec_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:ppc64le",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:ppc64le",
    ],
)

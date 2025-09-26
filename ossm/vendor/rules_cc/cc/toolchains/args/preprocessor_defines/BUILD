load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

package(default_visibility = ["//visibility:public"])

cc_feature(
    name = "feature",
    args = [":preprocessor_defines"],
    overrides = "//cc/toolchains/features/legacy:preprocessor_defines",
)

cc_args(
    name = "preprocessor_defines",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["-D{preprocessor_defines}"],
    format = {"preprocessor_defines": "//cc/toolchains/variables:preprocessor_defines"},
    iterate_over = "//cc/toolchains/variables:preprocessor_defines",
)

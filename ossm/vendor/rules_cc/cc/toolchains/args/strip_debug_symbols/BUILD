load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

package(default_visibility = ["//visibility:public"])

cc_feature(
    name = "feature",
    args = [":strip_debug_symbols"],
    overrides = "//cc/toolchains/features/legacy:strip_debug_symbols",
)

cc_args(
    name = "strip_debug_symbols",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["-Wl,-S"],
    requires_not_none = "//cc/toolchains/variables:strip_debug_symbols",
)

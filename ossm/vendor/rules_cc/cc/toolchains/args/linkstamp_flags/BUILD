load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "feature",
    args = [":flags"],
    overrides = "//cc/toolchains/features/legacy:linkstamps",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["{linkstamp_paths}"],
    format = {"linkstamp_paths": "//cc/toolchains/variables:linkstamp_paths"},
    iterate_over = "//cc/toolchains/variables:linkstamp_paths",
    requires_not_none = "//cc/toolchains/variables:linkstamp_paths",
)

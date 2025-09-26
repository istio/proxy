load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "feature",
    args = [
        ":objc_arc",
        ":no_objc_arc",
    ],
    feature_name = "_objc_arc",  # Doesn't override legacy feature, but shouldn't be disabled
    visibility = ["//visibility:public"],
)

cc_args(
    name = "objc_arc",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["-fobjc-arc"],
    requires_not_none = "//cc/toolchains/variables:objc_arc",
)

cc_args(
    name = "no_objc_arc",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["-fno-objc-arc"],
    requires_not_none = "//cc/toolchains/variables:no_objc_arc",
)

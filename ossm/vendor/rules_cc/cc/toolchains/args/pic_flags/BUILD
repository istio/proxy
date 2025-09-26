load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")

package(default_visibility = ["//visibility:private"])

cc_feature_set(
    name = "feature",
    all_of = [
        ":force_pic_feature",
        ":pic_feature",
    ],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "pic_feature",
    args = [":pic_flags"],
    overrides = "//cc/toolchains/features/legacy:pic",
)

cc_args(
    name = "pic_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["-fPIC"],
    requires_not_none = "//cc/toolchains/variables:pic",
)

cc_feature(
    name = "force_pic_feature",
    args = [":force_pic_flags"],
    overrides = "//cc/toolchains/features/legacy:force_pic_flags",
)

cc_args(
    name = "force_pic_flags",
    actions = ["//cc/toolchains/actions:link_executable_actions"],
    args = select({
        "@platforms//os:macos": ["-Wl,-pie"],
        "//conditions:default": ["-pie"],
    }),
    requires_not_none = "//cc/toolchains/variables:force_pic",
    visibility = ["//:__subpackages__"],
)

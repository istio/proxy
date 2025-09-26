load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")

cc_feature_set(
    name = "feature",
    all_of = [
        ":user_link_flags_feature",
        ":legacy_link_flags_feature",
        ":output_link_flags_feature",
    ],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "legacy_link_flags_feature",
    args = [":legacy_link_flags"],
    overrides = "//cc/toolchains/features/legacy:legacy_link_flags",
)

cc_args(
    name = "legacy_link_flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["{legacy_link_flags}"],
    format = {"legacy_link_flags": "//cc/toolchains/variables:legacy_link_flags"},
    iterate_over = "//cc/toolchains/variables:legacy_link_flags",
    requires_not_none = "//cc/toolchains/variables:legacy_link_flags",
)

cc_feature(
    name = "user_link_flags_feature",
    args = [":user_link_flags"],
    overrides = "//cc/toolchains/features/legacy:user_link_flags",
)

cc_args(
    name = "user_link_flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["{user_link_flags}"],
    format = {"user_link_flags": "//cc/toolchains/variables:user_link_flags"},
    iterate_over = "//cc/toolchains/variables:user_link_flags",
    requires_not_none = "//cc/toolchains/variables:user_link_flags",
)

cc_feature(
    name = "output_link_flags_feature",
    args = [":output_link_flags"],
    overrides = "//cc/toolchains/features/legacy:output_execpath_flags",
)

cc_args(
    name = "output_link_flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = [
        "-o",
        "{output_execpath}",
    ],
    format = {"output_execpath": "//cc/toolchains/variables:output_execpath"},
    requires_not_none = "//cc/toolchains/variables:output_execpath",
)

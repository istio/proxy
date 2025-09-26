load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:args_list.bzl", "cc_args_list")
load("//cc/toolchains:feature.bzl", "cc_feature")

package(default_visibility = ["//visibility:public"])

cc_feature(
    name = "feature",
    args = [":strip_flags"],
    # This does not override a feature since these arguments are directly bound
    # to the strip action in CppActionConfigs.java.
    feature_name = "strip_flags_feature",
)

cc_args_list(
    name = "strip_flags",
    args = [
        ":strip_debug",
        ":stripopts",
        ":preserve_dates",
        ":output_file",
        ":input_file",
    ],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "strip_debug",
    actions = ["//cc/toolchains/actions:strip"],
    args = ["-S"],
)

cc_args(
    name = "stripopts",
    actions = ["//cc/toolchains/actions:strip"],
    args = ["{stripopts}"],
    format = {"stripopts": "//cc/toolchains/variables:stripopts"},
    iterate_over = "//cc/toolchains/variables:stripopts",
)

cc_args(
    name = "preserve_dates",
    actions = ["//cc/toolchains/actions:strip"],
    args = select({
        "@platforms//os:macos": [],
        "//conditions:default": ["-p"],
    }),
)

cc_args(
    name = "output_file",
    actions = ["//cc/toolchains/actions:strip"],
    args = [
        "-o",
        "{output_file}",
    ],
    format = {"output_file": "//cc/toolchains/variables:output_file"},
    requires_not_none = "//cc/toolchains/variables:output_file",
)

cc_args(
    name = "input_file",
    actions = ["//cc/toolchains/actions:strip"],
    args = ["{input_file}"],
    format = {"input_file": "//cc/toolchains/variables:input_file"},
    requires_not_none = "//cc/toolchains/variables:input_file",
)

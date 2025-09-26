load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:args_list.bzl", "cc_args_list")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:nested_args.bzl", "cc_nested_args")

package(default_visibility = ["//visibility:private"])

bool_flag(
    name = "use_libtool_on_macos",
    build_setting_default = True,
)

config_setting(
    name = "use_libtool_on_macos_setting",
    constraint_values = ["@platforms//os:macos"],
    flag_values = {":use_libtool_on_macos": "true"},
)

cc_feature(
    name = "feature",
    args = [":archiver_flags"],
    overrides = "//cc/toolchains/features/legacy:archiver_flags",
    visibility = ["//visibility:public"],
)

cc_args_list(
    name = "archiver_flags",
    args = [
        ":create_static_archive",
        ":output_execpath",
        ":libraries_to_link",
    ],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "create_static_archive",
    actions = ["//cc/toolchains/actions:ar_actions"],
    args = select({
        ":use_libtool_on_macos_setting": ["-static"],
        "//conditions:default": ["rcsD"],
    }),
)

cc_args(
    name = "output_execpath",
    actions = ["//cc/toolchains/actions:ar_actions"],
    args = select({
        ":use_libtool_on_macos_setting": ["-o"],
        "//conditions:default": [],
    }) + ["{output_execpath}"],
    format = {"output_execpath": "//cc/toolchains/variables:output_execpath"},
    requires_not_none = "//cc/toolchains/variables:output_execpath",
)

cc_args(
    name = "libraries_to_link",
    actions = ["//cc/toolchains/actions:ar_actions"],
    nested = ["libraries_to_link_expansion"],
    requires_not_none = "//cc/toolchains/variables:libraries_to_link",
)

cc_nested_args(
    name = "libraries_to_link_expansion",
    iterate_over = "//cc/toolchains/variables:libraries_to_link",
    nested = [
        ":link_obj_file",
        ":link_object_file_group",
    ],
)

cc_nested_args(
    name = "link_obj_file",
    args = ["{object_file}"],
    format = {"object_file": "//cc/toolchains/variables:libraries_to_link.name"},
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "object_file",
)

cc_nested_args(
    name = "link_object_file_group",
    args = ["{object_files}"],
    format = {"object_files": "//cc/toolchains/variables:libraries_to_link.object_files"},
    iterate_over = "//cc/toolchains/variables:libraries_to_link.object_files",
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "object_file_group",
)

#
# To use this add the features to your toolchain:
#
# enabled_features = [
#     "@rules_cc//cc/toolchains/args/layering_check:module_maps",
# ],
# known_features = [
#     "@rules_cc//cc/toolchains/args/layering_check:layering_check",
#     "@rules_cc//cc/toolchains/args/layering_check:use_module_maps",
# ],
#
# And make sure you have 'module_map' set to a module map file that contains
# all your toolchain and sysroot files:
#
# module_map = ":builtin_module_map",
#

load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

package(default_visibility = ["//visibility:public"])

cc_feature(
    name = "module_maps",
    feature_name = "module_maps",
)

cc_feature(
    name = "use_module_maps",
    args = [
        ":dependent_module_map_files_args",
    ],
    feature_name = "use_module_maps",
    requires_any_of = [":module_maps"],
)

cc_feature(
    name = "layering_check",
    args = [
        ":layering_check_args",
        ":module_map_file_args",
        ":module_name_args",
    ],
    feature_name = "layering_check",
    implies = [":use_module_maps"],
)

cc_args(
    name = "layering_check_args",
    actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
    args = [
        "-fmodules-strict-decluse",
        "-Wprivate-header",
    ],
)

cc_args(
    name = "module_name_args",
    actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
    args = select({
        "@platforms//os:macos": ["-Xclang"],
        "//conditions:default": [],
    }) + ["-fmodule-name={module_name}"],
    format = {
        "module_name": "@rules_cc//cc/toolchains/variables:module_name",
    },
    requires_not_none = "@rules_cc//cc/toolchains/variables:module_name",
)

cc_args(
    name = "module_map_file_args",
    actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
    args = select({
        "@platforms//os:macos": ["-Xclang"],
        "//conditions:default": [],
    }) + ["-fmodule-map-file={module_map_file}"],
    format = {
        "module_map_file": "@rules_cc//cc/toolchains/variables:module_map_file",
    },
    requires_not_none = "@rules_cc//cc/toolchains/variables:module_map_file",
)

cc_args(
    name = "dependent_module_map_files_args",
    actions = ["@rules_cc//cc/toolchains/actions:compile_actions"],
    args = select({
        "@platforms//os:macos": ["-Xclang"],
        "//conditions:default": [],
    }) + [
        "-fmodule-map-file={dependent_module_map_files}",
    ],
    format = {
        "dependent_module_map_files": "@rules_cc//cc/toolchains/variables:dependent_module_map_files",
    },
    iterate_over = "@rules_cc//cc/toolchains/variables:dependent_module_map_files",
    requires_not_none = "@rules_cc//cc/toolchains/variables:dependent_module_map_files",
)

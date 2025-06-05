load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:nested_args.bzl", "cc_nested_args")
load("//cc/toolchains/args/libraries_to_link/private:library_link_args.bzl", "library_link_args")

package(default_visibility = ["//visibility:private"])

cc_feature(
    name = "feature",
    args = [":libraries_to_link"],
    overrides = "//cc/toolchains/features/legacy:libraries_to_link",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "libraries_to_link",
    actions = ["//cc/toolchains/actions:link_actions"],
    nested = [
        ":thinlto_param_file",
        ":libraries_to_link_args",
    ],
    visibility = ["//visibility:public"],
)

cc_nested_args(
    name = "thinlto_param_file",
    args = ["-Wl,@{param_file}"],
    format = {
        "param_file": "//cc/toolchains/variables:thinlto_param_file",
    },
    requires_not_none = "//cc/toolchains/variables:thinlto_param_file",
)

cc_nested_args(
    name = "libraries_to_link_args",
    nested = [":iterate_over_libraries_to_link"],
    requires_not_none = "//cc/toolchains/variables:libraries_to_link",
)

cc_nested_args(
    name = "iterate_over_libraries_to_link",
    iterate_over = "//cc/toolchains/variables:libraries_to_link",
    nested = [
        ":optional_object_file_group_start",
        ":single_library_args",
        ":optional_object_file_group_end",
    ],
)

cc_nested_args(
    name = "optional_object_file_group_start",
    nested = [":start_lib_arg"],
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "object_file_group",
)

cc_nested_args(
    name = "start_lib_arg",
    args = ["-Wl,--start-lib"],
    requires_false = "//cc/toolchains/variables:libraries_to_link.is_whole_archive",
)

cc_nested_args(
    name = "optional_object_file_group_end",
    nested = [":end_lib_arg"],
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "object_file_group",
)

cc_nested_args(
    name = "end_lib_arg",
    args = ["-Wl,--end-lib"],
    requires_false = "//cc/toolchains/variables:libraries_to_link.is_whole_archive",
)

cc_nested_args(
    name = "single_library_args",
    nested = select({
        "@platforms//os:macos": [],
        "//conditions:default": [":optional_whole_archive_start"],
    }) + [
        ":optional_object_file_group",
        ":optional_object_file",
        ":optional_interface_library",
        ":optional_static_library",
        ":optional_dynamic_library",
    ] + select({
        # maOS has a minor nuance where it uses the path to the library instead of `-l:{library_name}`.
        "@platforms//os:macos": [":macos_optional_versioned_dynamic_library"],
        "//conditions:default": [":generic_optional_versioned_dynamic_library"],
    }) + select({
        "@platforms//os:macos": [],
        "//conditions:default": [":optional_whole_archive_end"],
    }),
)

cc_nested_args(
    name = "optional_whole_archive_start",
    nested = [":whole_archive_start_arg"],
    requires_true = "//cc/toolchains/variables:libraries_to_link.is_whole_archive",
)

cc_nested_args(
    name = "whole_archive_start_arg",
    args = ["-Wl,-whole-archive"],
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "static_library",
)

cc_nested_args(
    name = "optional_whole_archive_end",
    nested = [":whole_archive_end_arg"],
    requires_true = "//cc/toolchains/variables:libraries_to_link.is_whole_archive",
)

cc_nested_args(
    name = "whole_archive_end_arg",
    args = ["-Wl,-no-whole-archive"],
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "static_library",
)

library_link_args(
    name = "optional_object_file_group",
    from_variable = "//cc/toolchains/variables:libraries_to_link.object_files",
    iterate_over_variable = True,
    library_type = "object_file_group",
)

library_link_args(
    name = "optional_object_file",
    from_variable = "//cc/toolchains/variables:libraries_to_link.name",
    library_type = "object_file",
)

library_link_args(
    name = "optional_interface_library",
    from_variable = "//cc/toolchains/variables:libraries_to_link.name",
    library_type = "interface_library",
)

library_link_args(
    name = "optional_static_library",
    from_variable = "//cc/toolchains/variables:libraries_to_link.name",
    library_type = "static_library",
)

cc_nested_args(
    name = "optional_dynamic_library",
    args = ["-l{library}"],
    format = {
        "library": "//cc/toolchains/variables:libraries_to_link.name",
    },
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "dynamic_library",
)

cc_nested_args(
    name = "generic_optional_versioned_dynamic_library",
    args = ["-l:{library_name}"],
    format = {
        "library_name": "//cc/toolchains/variables:libraries_to_link.name",
    },
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "versioned_dynamic_library",
)

cc_nested_args(
    name = "macos_optional_versioned_dynamic_library",
    args = ["{library_path}"],
    format = {
        "library_path": "//cc/toolchains/variables:libraries_to_link.path",
    },
    requires_equal = "//cc/toolchains/variables:libraries_to_link.type",
    requires_equal_value = "versioned_dynamic_library",
)

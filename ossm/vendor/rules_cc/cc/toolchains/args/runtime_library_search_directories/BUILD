load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:args_list.bzl", "cc_args_list")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_constraint.bzl", "cc_feature_constraint")
load("//cc/toolchains:nested_args.bzl", "cc_nested_args")

package(default_visibility = ["//visibility:private"])

cc_feature(
    name = "feature",
    args = [":runtime_library_search_directories"],
    overrides = "//cc/toolchains/features/legacy:runtime_library_search_directories",
    visibility = ["//visibility:public"],
)

# TODO: b/27153401 - The implementation of this is particularly complex because
# of what appears to be a workaround where macOS cc_test targets with
# static_link_cpp_runtimes enabled utilize a $EXEC_ORIGIN/ prefix. This can be
# paired down significantly after it is clear this workaround is no longer
# required.

cc_feature_constraint(
    name = "static_link_cpp_runtimes_enabled",
    all_of = ["//cc/toolchains/features:static_link_cpp_runtimes"],
)

cc_feature_constraint(
    name = "static_link_cpp_runtimes_disabled",
    none_of = ["//cc/toolchains/features:static_link_cpp_runtimes"],
)

cc_args_list(
    name = "runtime_library_search_directories",
    args = [
        ":runtime_library_search_directories_static_runtimes_args",
        ":runtime_library_search_directories_args",
    ],
    visibility = ["//visibility:public"],
)

cc_args(
    name = "runtime_library_search_directories_static_runtimes_args",
    actions = ["//cc/toolchains/actions:link_actions"],
    nested = [":iterate_over_search_dirs"],
    requires_any_of = [":static_link_cpp_runtimes_enabled"],
    requires_not_none = "//cc/toolchains/variables:runtime_library_search_directories",
)

cc_nested_args(
    name = "iterate_over_search_dirs",
    iterate_over = "//cc/toolchains/variables:runtime_library_search_directories",
    nested = [
        ":unit_test_static_runtime_search_dir_args",
        ":static_runtime_search_dir_args",
    ],
)

cc_nested_args(
    name = "unit_test_static_runtime_search_dir_args",
    args = [
        "-Xlinker",
        "-rpath",
        "-Xlinker",
        # TODO(b/27153401): This should probably be @loader_path on osx.
        "$EXEC_ORIGIN/{search_path}",
    ],
    format = {
        "search_path": "//cc/toolchains/variables:runtime_library_search_directories",
    },
    requires_true = "//cc/toolchains/variables:is_cc_test",
)

cc_nested_args(
    name = "static_runtime_search_dir_args",
    args = [
        "-Xlinker",
        "-rpath",
        "-Xlinker",
    ] + select({
        "@platforms//os:macos": ["@loader_path/{search_path}"],
        "//conditions:default": ["$ORIGIN/{search_path}"],
    }),
    format = {
        "search_path": "//cc/toolchains/variables:runtime_library_search_directories",
    },
    requires_false = "//cc/toolchains/variables:is_cc_test",
)

# TODO: b/27153401 - runtime_library_search_directories_args and
# search_dir_args are all we need to keep if the workaround is no
# longer required.
cc_args(
    name = "runtime_library_search_directories_args",
    actions = ["//cc/toolchains/actions:link_actions"],
    nested = [":search_dir_args"],
    # Remove the requires_any_of here if the workaround for b/27153401 is no
    # longer required.
    requires_any_of = [":static_link_cpp_runtimes_disabled"],
    requires_not_none = "//cc/toolchains/variables:runtime_library_search_directories",
)

cc_nested_args(
    name = "search_dir_args",
    args = [
        "-Xlinker",
        "-rpath",
        "-Xlinker",
    ] + select({
        "@platforms//os:macos": ["@loader_path/{search_path}"],
        "//conditions:default": ["$ORIGIN/{search_path}"],
    }),
    format = {
        "search_path": "//cc/toolchains/variables:runtime_library_search_directories",
    },
    iterate_over = "//cc/toolchains/variables:runtime_library_search_directories",
)

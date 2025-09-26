load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")

cc_feature_set(
    name = "feature",
    all_of = [
        ":includes_feature",
        ":include_paths_feature",
    ],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "includes_feature",
    args = [":includes_flags"],
    overrides = "//cc/toolchains/features/legacy:includes",
)

cc_args(
    name = "includes_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-include",
        "{includes}",
    ],
    format = {"includes": "//cc/toolchains/variables:includes"},
    iterate_over = "//cc/toolchains/variables:includes",
    requires_not_none = "//cc/toolchains/variables:includes",
)

cc_feature(
    name = "include_paths_feature",
    args = [
        ":quote_include_paths_flags",
        ":include_paths_flags",
        ":system_include_paths_flags",
        ":framework_include_paths_flags",
    ],
    overrides = "//cc/toolchains/features/legacy:include_paths",
)

cc_args(
    name = "quote_include_paths_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-iquote",
        "{quote_include_paths}",
    ],
    format = {"quote_include_paths": "//cc/toolchains/variables:quote_include_paths"},
    iterate_over = "//cc/toolchains/variables:quote_include_paths",
)

cc_args(
    name = "include_paths_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-I{include_paths}",
    ],
    format = {"include_paths": "//cc/toolchains/variables:include_paths"},
    iterate_over = "//cc/toolchains/variables:include_paths",
)

cc_args(
    name = "system_include_paths_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-isystem",
        "{system_include_paths}",
    ],
    format = {"system_include_paths": "//cc/toolchains/variables:system_include_paths"},
    iterate_over = "//cc/toolchains/variables:system_include_paths",
)

cc_args(
    name = "framework_include_paths_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-F{framework_include_paths}",
    ],
    format = {"framework_include_paths": "//cc/toolchains/variables:framework_include_paths"},
    iterate_over = "//cc/toolchains/variables:framework_include_paths",
)

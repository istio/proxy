load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")

cc_feature_set(
    name = "feature",
    all_of = [
        ":fission_feature",
        ":per_object_debug_info_feature",
    ],
    visibility = ["//visibility:public"],
)

cc_feature(
    name = "fission_feature",
    args = [":fission_flags"],
    overrides = "//cc/toolchains/features/legacy:fission_support",
)

cc_args(
    name = "fission_flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["-Wl,--gdb-index"],
    requires_not_none = "//cc/toolchains/variables:is_using_fission",
)

cc_feature(
    name = "per_object_debug_info_feature",
    args = [":per_object_debug_info_flags"],
    overrides = "//cc/toolchains/features/legacy:per_object_debug_info",
)

cc_args(
    name = "per_object_debug_info_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-gsplit-dwarf",
        "-g",
    ],
    requires_not_none = "//cc/toolchains/variables:per_object_debug_info_file",
)

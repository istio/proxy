load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:args_list.bzl", "cc_args_list")
load("//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "feature",
    args = [":flags"],
    overrides = "//cc/toolchains/features/legacy:compiler_output_flags",
    visibility = ["//visibility:public"],
)

cc_args_list(
    name = "flags",
    args = [
        ":output_assembly_flags",
        ":output_preprocess_flags",
        ":output_flags",
    ],
)

cc_args(
    name = "output_assembly_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["-S"],
    requires_not_none = "//cc/toolchains/variables:output_assembly_file",
)

cc_args(
    name = "output_preprocess_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = ["-E"],
    requires_not_none = "//cc/toolchains/variables:output_preprocess_file",
)

cc_args(
    name = "output_flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-o",
        "{output_file}",
    ],
    format = {"output_file": "//cc/toolchains/variables:output_file"},
    requires_not_none = "//cc/toolchains/variables:output_file",
)

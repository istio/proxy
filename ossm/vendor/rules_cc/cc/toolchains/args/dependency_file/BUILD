load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "feature",
    args = [":flags"],
    overrides = "//cc/toolchains/features/legacy:dependency_file",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "flags",
    actions = ["//cc/toolchains/actions:compile_actions"],
    args = [
        "-MD",
        "-MF",
        "{dependency_file}",
    ],
    format = {"dependency_file": "//cc/toolchains/variables:dependency_file"},
    requires_not_none = "//cc/toolchains/variables:dependency_file",
)

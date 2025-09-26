load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:feature_constraint.bzl", "cc_feature_constraint")

cc_feature_constraint(
    name = "static_link_cpp_runtimes_enabled",
    all_of = ["//cc/toolchains/features:static_link_cpp_runtimes"],
)

cc_feature(
    name = "feature",
    args = [":flags"],
    overrides = "//cc/toolchains/features/legacy:static_libgcc",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "flags",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["-static-libgcc"],
    requires_any_of = [":static_link_cpp_runtimes_enabled"],
)

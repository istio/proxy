load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

package(default_visibility = ["//visibility:private"])

cc_feature(
    name = "feature",
    args = [":shared_flag"],
    overrides = "//cc/toolchains/features/legacy:shared_flag",
    visibility = ["//visibility:public"],
)

cc_args(
    name = "shared_flag",
    actions = ["//cc/toolchains/actions:dynamic_library_link_actions"],
    args = ["-shared"],
    visibility = ["//visibility:public"],
)

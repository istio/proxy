load("//cc/toolchains:args.bzl", "cc_args")
load("//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "feature",
    args = select({
        "@platforms//os:macos": [":macos_set_install_name"],
        "//conditions:default": [],
    }),
    feature_name = "_soname_flags",  # Doesn't override legacy feature, but shouldn't be disabled
    visibility = ["//cc/toolchains/args:__pkg__"],
)

cc_args(
    name = "macos_set_install_name",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["-Wl,-install_name,@rpath/{runtime_solib_name}"],
    format = {"runtime_solib_name": "//cc/toolchains/variables:runtime_solib_name"},
    requires_not_none = "//cc/toolchains/variables:runtime_solib_name",
    target_compatible_with = ["@platforms//os:macos"],
)

cc_args(
    name = "set_soname",
    actions = ["//cc/toolchains/actions:link_actions"],
    args = ["-Wl,-soname,{runtime_solib_name}"],
    format = {"runtime_solib_name": "//cc/toolchains/variables:runtime_solib_name"},
    requires_not_none = "//cc/toolchains/variables:runtime_solib_name",
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

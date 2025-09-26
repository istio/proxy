load("@rules_license//rules:license.bzl", "license")

package(
    default_applicable_licenses = [":license"],
    default_visibility = ["//visibility:public"],
)

license(
    name = "license",
    license_kinds = [
        "@rules_license//licenses/spdx:Apache-2.0",
    ],
    license_text = "LICENSE",
)

exports_files([
    "LICENSE",
    "MODULE.bazel",
])

filegroup(
    name = "srcs",
    srcs = [
        "BUILD",
        "WORKSPACE",
        "//cpu:srcs",
        "//os:srcs",
        "//host:srcs",
    ],
)

# For use in Incompatible Target Skipping:
# https://docs.bazel.build/versions/main/platforms.html#skipping-incompatible-targets
#
# Specifically this lets targets declare incompatibility with some set of
# platforms. See
# https://docs.bazel.build/versions/main/platforms.html#more-expressive-constraints
# for some more details.
constraint_setting(name = "incompatible_setting")

constraint_value(
    name = "incompatible",
    constraint_setting = ":incompatible_setting",
)

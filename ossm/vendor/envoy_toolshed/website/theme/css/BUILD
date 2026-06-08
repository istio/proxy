load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files")

pkg_files(
    name = "scss",
    srcs = [],
    prefix = "theme/static/css/_sass/vendor/bootstrap",
    strip_prefix = "scss",
    visibility = ["//visibility:public"],
)

pkg_files(
    name = "css_files",
    srcs = glob(["**/*css"]),
    prefix = "theme/static/css",
    strip_prefix = "",
    visibility = ["//visibility:public"],
)

pkg_filegroup(
    name = "css",
    srcs = [
        ":css_files",
        ":scss",
    ],
    visibility = ["//visibility:public"],
)

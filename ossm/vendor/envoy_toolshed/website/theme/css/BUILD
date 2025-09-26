load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files")

pkg_files(
    name = "scss",
    srcs = [],
    strip_prefix = "scss",
    prefix = "theme/static/css/_sass/vendor/bootstrap",
    visibility = ["//visibility:public"],
)

pkg_files(
    name = "css_files",
    srcs = glob(["**/*css"]),
    strip_prefix = "",
    prefix = "theme/static/css",
    visibility = ["//visibility:public"],
)

pkg_filegroup(
    name = "css",
    srcs = [
        ":scss",
        ":css_files",
    ],
    visibility = ["//visibility:public"],
)

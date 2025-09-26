load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files")
load("@rules_pkg//pkg:pkg.bzl", "pkg_tar")

pkg_files(
    name = "image_files",
    srcs = glob([
        "**/*.ico",
        "**/*.svg",
        "**/*.png",
    ]),
    strip_prefix = "",
    prefix = "theme/static/images",
    visibility = ["//visibility:public"],
)

pkg_filegroup(
    name = "images",
    srcs = [":image_files"],
    visibility = ["//visibility:public"],
)

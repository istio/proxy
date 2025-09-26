load("@rules_pkg//pkg:mappings.bzl", "pkg_filegroup", "pkg_files")
load("@rules_pkg//pkg:pkg.bzl", "pkg_tar")

pkg_files(
    name = "templates",
    srcs = glob(["*html"]),
    prefix = "theme/templates",
    visibility = ["//visibility:public"],
)

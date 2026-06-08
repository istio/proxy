load("@aspect_rules_js//npm:defs.bzl", "npm_package")

npm_package(
    name = "pkg",
    srcs = glob(
        include = ["package/**/*"],
    ),
    include_runfiles = False,
    root_paths = ["package"],
    visibility = ["//visibility:public"],
)

load("@io_bazel_stardoc//stardoc:stardoc.bzl", "stardoc")

exports_files(
    [
        "includes/main_functions_header.md",
        "includes/spec_functions_header.md",
    ],
    visibility = ["//scripts:__pkg__"],
)

stardoc(
    name = "defs",
    out = "defs.md",
    input = "//:defs.bzl",
    symbol_names = [
        "javadoc",
        "java_export",
        "maven_bom",
        "maven_install",
    ],
    visibility = ["//scripts:__pkg__"],
    deps = [
        "//:implementation",
        "@bazel_skylib//lib:structs",
    ],
)

stardoc(
    name = "specs",
    out = "specs.md",
    input = "//:specs.bzl",
    symbol_names = [
        "maven.artifact",
        "maven.repository",
        "maven.exclusion",
    ],
    visibility = ["//scripts:__pkg__"],
    deps = [
        "//:implementation",
        "@bazel_skylib//lib:structs",
    ],
)

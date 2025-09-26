load("@bazel_gazelle//:def.bzl", "gazelle", "gazelle_binary")
load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@io_bazel_stardoc//stardoc:stardoc.bzl", "stardoc")

_DOC_SRCS = {
    "api": [
        "swift_common",
    ],
    "providers": [
        "SwiftInfo",
        "SwiftToolchainInfo",
        "SwiftProtoCompilerInfo",
        "SwiftProtoInfo",
    ],
    "rules": [
        "swift_binary",
        "swift_compiler_plugin",
        "universal_swift_compiler_plugin",
        "swift_compiler_plugin_import",
        "swift_cross_import_overlay",
        "swift_feature_allowlist",
        "swift_import",
        "swift_interop_hint",
        "swift_library",
        "swift_library_group",
        "mixed_language_library",
        "swift_module_alias",
        "swift_module_mapping",
        "swift_module_mapping_test",
        "swift_package_configuration",
        "swift_test",
        "swift_proto_library",
        "swift_proto_library_group",
        "swift_proto_compiler",
        "deprecated_swift_grpc_library",
        "deprecated_swift_proto_library",
    ],
}

write_file(
    name = "api_header",
    out = "api_header.vm",
    content = [
        "<!-- Generated with Stardoc, Do Not Edit! -->",
        "# Build API",
        "",
        "The `swift_common` module provides API access to the behavior implemented",
        "by the Swift build rules, so that other custom rules can invoke Swift",
        "compilation and/or linking as part of their implementation.",
    ],
)

write_file(
    name = "rules_header",
    out = "rules_header.vm",
    content = [
        "<!-- Generated with Stardoc, Do Not Edit! -->",
        "",
        "${moduleDocstring}",
        "On this page:",
        "",
    ] + ["  * [{0}](#{0})".format(r) for r in _DOC_SRCS["rules"]] + [
        "",
    ],
)

write_file(
    name = "providers_header",
    out = "providers_header.vm",
    content = [
        "<!-- Generated with Stardoc, Do Not Edit! -->",
        "",
        "The providers described below are propagated and required by various Swift",
        "build rules. Clients interested in writing custom rules that interface",
        "with the rules in this package should use these providers to communicate",
        "with the Swift build rules as needed.",
        "",
        "On this page:",
        "",
    ] + ["  * [{0}](#{0})".format(r) for r in _DOC_SRCS["providers"]] + [
        "",
    ],
)

write_file(
    name = "setup_header",
    out = "setup_header.vm",
    content = [
        "<!-- Generated with Stardoc, Do Not Edit! -->",
        "# Workspace Setup",
    ],
)

[
    stardoc(
        name = file + "_doc",
        out = file + ".md_",
        header_template = file + "_header.vm",
        input = "//doc:doc.bzl",
        symbol_names = symbols,
        deps = ["//doc"],
    )
    for [
        file,
        symbols,
    ] in _DOC_SRCS.items()
]

stardoc(
    name = "setup_doc",
    out = "setup.md_",
    header_template = "setup_header.vm",
    input = "//swift:repositories.bzl",
    deps = ["//swift:repositories"],
)

# To make these tests pass, run
# bazel run //doc:update
[
    diff_test(
        name = "test_" + file,
        file1 = file + ".md_",
        file2 = file + ".md",
    )
    for file in _DOC_SRCS.keys() + ["setup"]
]

write_file(
    name = "gen_update",
    out = "update.sh",
    content = [
        "#!/usr/bin/env bash",
        "cd $BUILD_WORKSPACE_DIRECTORY",
    ] + [
        "cp -fv bazel-bin/doc/{0}.md_ doc/{0}.md".format(
            file,
        )
        for file in _DOC_SRCS.keys() + ["setup"]
    ],
)

sh_binary(
    name = "update",
    srcs = ["update.sh"],
    data = [file + ".md_" for file in _DOC_SRCS.keys() + ["setup"]],
)

gazelle(
    name = "gazelle",
    gazelle = ":gazelle_bin",
    visibility = ["//visibility:private"],
)

gazelle_binary(
    name = "gazelle_bin",
    languages = [
        "@bazel_skylib_gazelle_plugin//bzl",
    ],
    visibility = ["//visibility:private"],
)

# Have to keep since bazel doesn't propagate a bzl_library for these files
# gazelle:ignore
bzl_library(
    name = "bazel_tools_bzl",
    srcs = ["@bazel_tools//tools:bzl_srcs"],
    visibility = ["//swift:__pkg__"],
)

# gazelle:ignore
bzl_library(
    name = "doc",
    srcs = ["//doc:doc.bzl"],
    visibility = ["//doc:__pkg__"],
    deps = [
        "//mixed_language",
        "//proto",
        "//swift",
    ],
)

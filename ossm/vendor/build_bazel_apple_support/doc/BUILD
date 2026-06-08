load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@io_bazel_stardoc//stardoc:stardoc.bzl", "stardoc")

_DOC_SRCS = {
    "rules": [
        "apple_genrule",
        "toolchain_substitution",
        "universal_binary",
    ],
}

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

[
    stardoc(
        name = file + "_doc",
        out = file + ".md_",
        header_template = file + "_header.vm",
        input = "//rules:rules.doc.bzl",
        symbol_names = symbols,
        tags = ["no-sandbox"],  # https://github.com/bazelbuild/stardoc/issues/112
        deps = ["//rules"],
    )
    for [
        file,
        symbols,
    ] in _DOC_SRCS.items()
]

[
    diff_test(
        name = "test_" + file,
        failure_message = "\nPlease update the docs by running\n    bazel run //doc:update",
        file1 = file + ".md_",
        file2 = file + ".md",
    )
    for file in _DOC_SRCS.keys()
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
        for file in _DOC_SRCS.keys()
    ],
)

sh_binary(
    name = "update",
    srcs = ["update.sh"],
    data = [file + ".md_" for file in _DOC_SRCS.keys()],
)

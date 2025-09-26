"""Turn a label_list of mixed sources and bzl_library's into a bzl_library.

The sources can be anything. Only the ones that end in ".bzl" will be added.
"""

load("@bazel_skylib//:bzl_library.bzl", "StarlarkLibraryInfo")

def _make_starlark_library(ctx):
    direct = []
    transitive = []
    for src in ctx.attr.srcs:
        if StarlarkLibraryInfo in src:
            transitive.append(src[StarlarkLibraryInfo])
        else:
            for file in src[DefaultInfo].files.to_list():
                if file.path.endswith(".bzl"):
                    # print(file.path)
                    direct.append(file)
    all_files = depset(direct, transitive = transitive)
    return [
        DefaultInfo(files = all_files, runfiles = ctx.runfiles(transitive_files = all_files)),
        StarlarkLibraryInfo(srcs = direct, transitive_srcs = all_files),
    ]

starlark_library = rule(
    implementation = _make_starlark_library,
    attrs = {
        "srcs": attr.label_list(
            doc = "Any mix of source files. Only .bzl files will be used.",
            allow_files = True,
            cfg = "exec",
            mandatory = True,
        ),
    },
)

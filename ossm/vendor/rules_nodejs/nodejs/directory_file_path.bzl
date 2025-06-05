"""This module contains providers for working with TreeArtifacts.

See https://github.com/bazelbuild/bazel-skylib/issues/300
(this feature could be upstreamed to bazel-skylib in the future)

These are also called output directories, created by `ctx.actions.declare_directory`.
"""

load("//nodejs/private/providers:directory_file_path_info.bzl", "DirectoryFilePathInfo")

def _directory_file_path(ctx):
    if not ctx.file.directory.is_directory:
        fail("directory attribute must be created with ctx.declare_directory (TreeArtifact)")
    return [DirectoryFilePathInfo(path = ctx.attr.path, directory = ctx.file.directory)]

directory_file_path = rule(
    doc = """Provide DirectoryFilePathInfo to reference some file within a directory.

        Otherwise there is no way to give a Bazel label for it.""",
    implementation = _directory_file_path,
    attrs = {
        "directory": attr.label(
            doc = "a directory",
            mandatory = True,
            allow_single_file = True,
        ),
        "path": attr.string(
            doc = "a path within that directory",
            mandatory = True,
        ),
    },
)

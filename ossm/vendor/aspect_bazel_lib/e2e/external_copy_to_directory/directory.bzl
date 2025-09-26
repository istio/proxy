"""Test rule executing `copy_to_directory_bin_action`."""

load("@aspect_bazel_lib//lib:copy_to_directory.bzl", "copy_to_directory_bin_action")

def _directory_impl(ctx):
    dst = ctx.actions.declare_directory(ctx.attr.name)

    copy_to_directory_bin_action(
        ctx,
        name = ctx.attr.name,
        copy_to_directory_bin = ctx.executable._tool,
        dst = dst,
        files = ctx.files.srcs,
        verbose = True,
    )

    return DefaultInfo(files = depset([dst]))

directory = rule(
    implementation = _directory_impl,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "_tool": attr.label(
            executable = True,
            cfg = "exec",
            default = "@aspect_bazel_lib//tools/copy_to_directory",
        ),
    },
    doc = """
        Copies the given source files to a directory with
        `copy_to_directory_bin_action()`.
    """,
)

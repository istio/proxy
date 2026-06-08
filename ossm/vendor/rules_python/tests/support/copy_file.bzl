"""Copies a file to a directory."""

def _copy_file_to_dir_impl(ctx):
    out_file = ctx.actions.declare_file(
        "{}/{}".format(ctx.attr.out_dir, ctx.file.src.basename),
    )
    ctx.actions.run_shell(
        inputs = [ctx.file.src],
        outputs = [out_file],
        arguments = [ctx.file.src.path, out_file.path],
        # Perform a copy to better match how a file install from
        # a repo-phase (e.g. whl extraction) looks.
        command = 'cp -f "$1" "$2"',
        progress_message = "Copying %{input} to %{output}",
    )
    return [DefaultInfo(files = depset([out_file]))]

copy_file_to_dir = rule(
    implementation = _copy_file_to_dir_impl,
    doc = """
This allows copying a file whose name is platform-dependent to a directory.

While bazel_skylib has a copy_file rule, you must statically specify the
output file name.
""",
    attrs = {
        "out_dir": attr.string(mandatory = True),
        "src": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
    },
)

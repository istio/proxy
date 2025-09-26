"""Rule that uses copy actions"""

load("@aspect_bazel_lib//lib:copy_file.bzl", "COPY_FILE_TOOLCHAINS", "copy_file_action")

def _simple_copy_file_impl(ctx):
    if len(ctx.files.src) != 1:
        fail("src must be a single file")
    if ctx.files.src[0].is_directory:
        fail("cannot use copy_file on a directory; try copy_directory instead")

    copy_file_action(ctx, ctx.files.src[0], ctx.outputs.out)

    files = depset(direct = [ctx.outputs.out])
    runfiles = ctx.runfiles(files = [ctx.outputs.out])

    return [DefaultInfo(files = files, runfiles = runfiles)]

simple_copy_file = rule(
    implementation = _simple_copy_file_impl,
    provides = [DefaultInfo],
    attrs = {
        "src": attr.label(mandatory = True, allow_files = True),
        "out": attr.output(mandatory = True),
    },
    toolchains = COPY_FILE_TOOLCHAINS,
)

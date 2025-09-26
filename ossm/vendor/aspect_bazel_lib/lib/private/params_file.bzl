"params_file rule"

load(":expand_variables.bzl", "expand_variables")
load(":strings.bzl", "split_args")

_ATTRS = {
    "args": attr.string_list(),
    "data": attr.label_list(allow_files = True),
    "newline": attr.string(
        values = ["unix", "windows", "auto"],
        default = "auto",
    ),
    "out": attr.output(mandatory = True),
    "_windows_constraint": attr.label(default = "@platforms//os:windows"),
}

def _params_file_impl(ctx):
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])

    if ctx.attr.newline == "auto":
        newline = "\r\n" if is_windows else "\n"
    elif ctx.attr.newline == "windows":
        newline = "\r\n"
    else:
        newline = "\n"

    expanded_args = []

    # Expand predefined source/output path && predefined variables & custom variables
    for a in ctx.attr.args:
        expanded_args += split_args(expand_variables(ctx, ctx.expand_location(a, targets = ctx.attr.data), outs = [ctx.outputs.out]))

    # ctx.actions.write creates a FileWriteAction which uses UTF-8 encoding.
    ctx.actions.write(
        output = ctx.outputs.out,
        content = newline.join(expanded_args),
        is_executable = False,
    )
    files = depset(direct = [ctx.outputs.out])
    runfiles = ctx.runfiles(files = [ctx.outputs.out])
    return [DefaultInfo(files = files, runfiles = runfiles)]

params_file = rule(
    implementation = _params_file_impl,
    provides = [DefaultInfo],
    attrs = _ATTRS,
)

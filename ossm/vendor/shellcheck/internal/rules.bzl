"""This file provides all user facing functions.
"""

def _impl_test(ctx):
    cmd = [ctx.file._shellcheck.short_path]
    if ctx.attr.format:
        cmd.append("--format={}".format(ctx.attr.format))
    if ctx.attr.severity:
        cmd.append("--severity={}".format(ctx.attr.severity))
    cmd += [f.short_path for f in ctx.files.data]
    cmd = " ".join(cmd)

    if ctx.attr.expect_fail:
        script = "{cmd} || exit 0\nexit1".format(cmd = cmd)
    else:
        script = "exec {cmd}".format(cmd = cmd)

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )

    return [
        DefaultInfo(
            executable = ctx.outputs.executable,
            runfiles = ctx.runfiles(files = [ctx.file._shellcheck] + ctx.files.data),
        ),
    ]

shellcheck_test = rule(
    implementation = _impl_test,
    attrs = {
        "data": attr.label_list(
            allow_files = True,
        ),
        "expect_fail": attr.bool(
            default = False,
        ),
        "format": attr.string(
            values = ["checkstyle", "diff", "gcc", "json", "json1", "quiet", "tty"],
            doc = "The format of the outputted lint results.",
        ),
        "severity": attr.string(
            values = ["error", "info", "style", "warning"],
            doc = "The severity of the lint results.",
        ),
        "_shellcheck": attr.label(
            default = Label("//:shellcheck"),
            allow_single_file = True,
            cfg = "exec",
            executable = True,
            doc = "The shellcheck executable to use.",
        ),
    },
    test = True,
)

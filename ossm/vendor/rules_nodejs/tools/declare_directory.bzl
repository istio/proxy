"Minimal test fixture to create a directory output"

def _impl(ctx):
    dir = ctx.actions.declare_directory(ctx.label.name)
    ctx.actions.run_shell(
        inputs = ctx.files.srcs,
        outputs = [dir],
        # RBE requires that we mkdir, but outside RBE it might already exist
        command = "mkdir -p {0}; cp $@ {0}".format(dir.path),
        arguments = [s.path for s in ctx.files.srcs],
    )
    return [
        DefaultInfo(files = depset([dir])),
    ]

declare_directory = rule(_impl, attrs = {"srcs": attr.label_list(allow_files = True)})

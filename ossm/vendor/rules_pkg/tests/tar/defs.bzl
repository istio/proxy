"""Helpers for testing tar packaging."""

def _raw_symlinks_impl(ctx):
    link1 = ctx.actions.declare_symlink(ctx.label.name + "_link1")
    ctx.actions.symlink(output = link1, target_path = "./link1")

    runfile_link = ctx.actions.declare_symlink(ctx.label.name + "_runfile_link")
    ctx.actions.symlink(output = runfile_link, target_path = "../runfile_link")

    return [DefaultInfo(
        files = depset([link1]),
        runfiles = ctx.runfiles([runfile_link]),
    )]

raw_symlinks = rule(
    implementation = _raw_symlinks_impl,
)

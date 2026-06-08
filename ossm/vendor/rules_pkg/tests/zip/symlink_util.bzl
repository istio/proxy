"""Helpers for testing zip packaging."""

def _create_fake_symlink_impl(ctx):
    symlink = ctx.actions.declare_symlink(ctx.attr.link)
    ctx.actions.symlink(output = symlink, target_path = ctx.attr.target)
    return [DefaultInfo(files = depset([symlink]))]

create_fake_symlink = rule(
    implementation = _create_fake_symlink_impl,
    attrs = {
        "link": attr.string(mandatory = True),
        "target": attr.string(mandatory = True),
    },
)

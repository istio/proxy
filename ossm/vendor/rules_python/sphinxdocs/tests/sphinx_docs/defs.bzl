"""Supporting code for tests."""

def _gen_directory_impl(ctx):
    out = ctx.actions.declare_directory(ctx.label.name)

    ctx.actions.run_shell(
        outputs = [out],
        command = """
echo "# Hello" > {outdir}/index.md
""".format(
            outdir = out.path,
        ),
    )

    return [DefaultInfo(files = depset([out]))]

gen_directory = rule(
    implementation = _gen_directory_impl,
)

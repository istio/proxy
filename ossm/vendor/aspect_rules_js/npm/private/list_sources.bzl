"Output a list of source files to a file"

def _list_sources_impl(ctx):
    output = ctx.actions.declare_file(ctx.label.name)
    content = "\n".join([file.path for file in ctx.files.srcs]) + "\n"

    ctx.actions.write(
        output,
        content,
    )

    return DefaultInfo(files = depset([output]), runfiles = ctx.runfiles(files = [output]))

list_sources = rule(
    attrs = {
        "srcs": attr.label_list(allow_files = True),
    },
    implementation = _list_sources_impl,
)

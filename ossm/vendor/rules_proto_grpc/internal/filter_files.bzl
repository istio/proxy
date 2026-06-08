"""Rule to filter files in the default output files by a file extension."""

def _filter_files_impl(ctx):
    """Filter the files in DefaultInfo."""
    return [DefaultInfo(
        files = depset([
            file
            for file in ctx.attr.target.files.to_list()
            if file.extension in ctx.attr.extensions
        ]),
    )]

filter_files = rule(
    implementation = _filter_files_impl,
    attrs = {
        "target": attr.label(
            doc = "The source target to filter",
            mandatory = True,
        ),
        "extensions": attr.string_list(
            doc = "The extensions of the files to keep eg. ['h']",
            mandatory = True,
        ),
    },
)

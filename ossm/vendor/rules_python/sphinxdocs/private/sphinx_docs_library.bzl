"""Implementation of sphinx_docs_library."""

load(":sphinx_docs_library_info.bzl", "SphinxDocsLibraryInfo")

def _sphinx_docs_library_impl(ctx):
    strip_prefix = ctx.attr.strip_prefix or (ctx.label.package + "/")
    direct_entries = []
    if ctx.files.srcs:
        entry = struct(
            strip_prefix = strip_prefix,
            prefix = ctx.attr.prefix,
            files = ctx.files.srcs,
        )
        direct_entries.append(entry)

    return [
        SphinxDocsLibraryInfo(
            strip_prefix = strip_prefix,
            prefix = ctx.attr.prefix,
            files = ctx.files.srcs,
            transitive = depset(
                direct = direct_entries,
                transitive = [t[SphinxDocsLibraryInfo].transitive for t in ctx.attr.deps],
            ),
        ),
        DefaultInfo(
            files = depset(ctx.files.srcs),
        ),
    ]

sphinx_docs_library = rule(
    implementation = _sphinx_docs_library_impl,
    attrs = {
        "deps": attr.label_list(
            doc = """
Additional `sphinx_docs_library` targets to include. They do not have the
`prefix` and `strip_prefix` attributes applied to them.""",
            providers = [SphinxDocsLibraryInfo],
        ),
        "prefix": attr.string(
            doc = "Prefix to prepend to file paths. Added after `strip_prefix` is removed.",
        ),
        "srcs": attr.label_list(
            allow_files = True,
            doc = "Files that are part of the library.",
        ),
        "strip_prefix": attr.string(
            doc = "Prefix to remove from file paths. Removed before `prefix` is prepended.",
        ),
    },
)

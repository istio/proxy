"npm_package_internal rule"

load(":npm_package_info.bzl", "NpmPackageInfo")

_ATTRS = {
    "src": attr.label(
        doc = "A source directory or output directory to use for this package.",
        allow_single_file = True,
        mandatory = True,
    ),
    "package": attr.string(
        doc = """The package name.""",
        mandatory = True,
    ),
    "version": attr.string(
        doc = """The package version.""",
        mandatory = True,
    ),
}

def _npm_package_internal_impl(ctx):
    if ctx.file.src.is_source or ctx.file.src.is_directory:
        # pass the source archive, source directory or TreeArtifact through
        dst = ctx.file.src
    else:
        fail("Expected src to be a source directory or an output directory")

    return [
        DefaultInfo(
            files = depset([dst]),
        ),
        NpmPackageInfo(
            package = ctx.attr.package,
            version = ctx.attr.version,
            # TODO(2.0): rename `directory` to `src` since it may now be an archive file
            directory = dst,
            npm_package_store_deps = depset(),
        ),
    ]

npm_package_internal = rule(
    implementation = _npm_package_internal_impl,
    attrs = _ATTRS,
    provides = [DefaultInfo, NpmPackageInfo],
)

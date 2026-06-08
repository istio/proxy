"""Repository rule for LLVM prebuilt distributions (cross-compilation libc++)."""

def _llvm_prebuilt_impl(ctx):
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        stripPrefix = ctx.attr.strip_prefix,
    )
    ctx.file("BUILD.bazel", ctx.attr.build_file_content)

llvm_prebuilt = repository_rule(
    implementation = _llvm_prebuilt_impl,
    attrs = {
        "build_file_content": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
        "url": attr.string(mandatory = True),
    },
)

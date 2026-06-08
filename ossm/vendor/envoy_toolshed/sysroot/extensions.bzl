"""Module extension for sysroot configuration in bzlmod."""

load(":sysroot.bzl", "sysroot", "setup_sysroots")

def _sysroot_impl(module_ctx):
    """Implementation of the sysroot module extension.

    This extension allows configuring sysroots in MODULE.bazel using the same
    setup_sysroots() function used in WORKSPACE.
    """

    # Collect all setup tags from all modules
    has_tags = False
    for mod in module_ctx.modules:
        if mod.tags.setup:
            has_tags = True
            break

    # If no tags specified, use default configuration
    if not has_tags:
        setup_sysroots()
    else:
        # Process each setup tag from all modules
        for mod in module_ctx.modules:
            for setup_tag in mod.tags.setup:
                setup_sysroots(
                    version = setup_tag.version if setup_tag.version else None,
                    glibc_version = setup_tag.glibc_version,
                    stdcc_version = setup_tag.stdcc_version if setup_tag.stdcc_version else None,
                    name_prefix = setup_tag.name_prefix,
                )

_setup = tag_class(
    attrs = {
        "version": attr.string(
            doc = "Version of sysroot release to use (default: uses VERSIONS['bins_release'])",
        ),
        "glibc_version": attr.string(
            default = "2.31",
            doc = "glibc version to use (default: '2.31', also available: '2.28')",
        ),
        "stdcc_version": attr.string(
            default = "13",
            doc = "libstdc++ version to use (default: '13', set to empty string for base sysroot)",
        ),
        "name_prefix": attr.string(
            default = "",
            doc = """Optional prefix for sysroot repository names (default: '').
                    Allows multiple sysroot setups, e.g., name_prefix='old' creates
                    @old_sysroot_linux_amd64 and @old_sysroot_linux_arm64""",
        ),
    },
)

sysroot_extension = module_extension(
    implementation = _sysroot_impl,
    tag_classes = {
        "setup": _setup,
    },
)

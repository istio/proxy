"""Module extension for grcov configuration in bzlmod."""

load(":grcov_repository.bzl", "grcov_repository")

def _grcov_impl(module_ctx):
    """Implementation of the grcov module extension.

    This extension allows configuring grcov in MODULE.bazel using the same
    grcov_repository() function used in WORKSPACE.
    """

    # Collect all setup tags from all modules
    # Only use the first tag found (grcov repo has a fixed name)
    setup_tag = None
    for mod in module_ctx.modules:
        for tag in mod.tags.setup:
            if setup_tag == None:
                setup_tag = tag
            else:
                # Fail if multiple tags are found
                fail("Multiple setup() calls found for grcov_extension. Only one configuration is allowed since repository name is fixed to @grcov.")

    # Call grcov_repository once
    # No custom configuration is needed currently - the function takes no parameters
    grcov_repository()

_setup = tag_class(
    attrs = {},
)

grcov_extension = module_extension(
    implementation = _grcov_impl,
    tag_classes = {
        "setup": _setup,
    },
)

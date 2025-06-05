"""A helper module for the various targets in the `@rules_rust//tools` package"""

def aspect_repository():
    """Determines the repository name to use in Bazel commands that use aspects.

    Some tools (`//tools/rustfmt` `//tools/rust_analyzer`) make calls to Bazel
    and pass the `--aspects` flag. This macro allows those tools to work around
    the following issue: https://github.com/bazelbuild/bazel/issues/11734

    Returns:
        str: The string to use for the `--aspects` repository labels
    """
    if native.repository_name() == "@":
        return ""
    return "@" + native.repository_name()

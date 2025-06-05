"""A rule that copies a directory to another place.

The rule uses a precompiled binary to perform the copy, so no shell is required.

## Preserving modification times

`copy_directory` and `copy_to_directory` have a `preserve_mtime` attribute, however
there are two caveats to consider when using this feature:

1. Remote Execution / Caching: These layers will reset the modify time and are
    incompatible with this feature. To avoid these failures the [no-remote tag](https://bazel.build/reference/be/common-definitions)
    can be added.
2. Caching: Changes to only the modified time will not re-trigger cached actions. This can
    be worked around by using a clean build when these types of changes occur. For tests the
    [external tag](https://bazel.build/reference/be/common-definitions) can be used but this
    will result in tests never being cached.
"""

load(
    "//lib/private:copy_directory.bzl",
    _copy_directory = "copy_directory",
    _copy_directory_bin_action = "copy_directory_bin_action",
)

copy_directory = _copy_directory
copy_directory_bin_action = _copy_directory_bin_action

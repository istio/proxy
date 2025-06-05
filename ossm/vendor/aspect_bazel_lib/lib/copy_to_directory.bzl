"""Copy files and directories to an output directory.
"""

load(
    "//lib/private:copy_to_directory.bzl",
    _copy_to_directory_bin_action = "copy_to_directory_bin_action",
    _copy_to_directory_lib = "copy_to_directory_lib",
)

# export the starlark library as a public API
copy_to_directory_lib = _copy_to_directory_lib
copy_to_directory_bin_action = _copy_to_directory_bin_action

copy_to_directory = rule(
    doc = _copy_to_directory_lib.doc,
    implementation = _copy_to_directory_lib.impl,
    provides = _copy_to_directory_lib.provides,
    attrs = _copy_to_directory_lib.attrs,
    toolchains = ["@aspect_bazel_lib//lib:copy_to_directory_toolchain_type"],
)

"""Re-export of https://registry.bazel.build/modules/tar.bzl to avoid breaking change.
https://github.com/bazel-contrib/bazel-lib/pull/1083 moved these symbols to tar.bzl
TODO(3.0): delete
"""

load("@tar.bzl//tar:mtree.bzl", _mtree_mutate = "mtree_mutate", _mtree_spec = "mtree_spec")
load("@tar.bzl//tar:tar.bzl", _tar = "tar", _tar_lib = "tar_lib", _tar_rule = "tar_rule")

tar_lib = struct(
    attrs = _tar_lib.attrs,
    implementation = _tar_lib.implementation,
    mtree_attrs = _tar_lib.mtree_attrs,
    mtree_implementation = _tar_lib.mtree_implementation,
    toolchain_type = "@aspect_bazel_lib//lib:tar_toolchain_type",
    common = _tar_lib.common,
)

mtree_mutate = _mtree_mutate
mtree_spec = _mtree_spec
tar = _tar
tar_rule = _tar_rule

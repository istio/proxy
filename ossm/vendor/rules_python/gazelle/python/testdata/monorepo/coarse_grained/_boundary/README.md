# \_boundary

This Bazel package must be before other packages in the `coarse_grained`
directory so that we assert that walking the tree still happens after ignoring
this package from the parent coarse-grained generation.

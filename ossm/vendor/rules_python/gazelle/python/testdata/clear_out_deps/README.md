# Clearing deps / pyi_deps

This test case asserts that an existing `py_library` specifying `deps` and
`pyi_deps` have these attributes removed if the corresponding imports are
removed.

`a/BUILD.in` declares `deps`/`pyi_deps` on non-existing libraries, `b/BUILD.in` declares dependency on `//a`
without a matching import, and `c/BUILD.in` declares both `deps` and `pyi_deps` as `["//a", "//b"]`, but
it should have only `//a` as `deps` and only `//b` as `pyi_deps`.

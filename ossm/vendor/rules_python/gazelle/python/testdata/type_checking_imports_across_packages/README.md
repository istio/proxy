# Overlapping deps and pyi_deps across packages

This test reproduces a case where a dependency may be added to both `deps` and
`pyi_deps`. Package `b` imports `a.foo` normally and imports `a.bar` as a
type-checking only import. The dependency on package `a` should appear only in
`deps` (and not `pyi_deps`) of package `b`.

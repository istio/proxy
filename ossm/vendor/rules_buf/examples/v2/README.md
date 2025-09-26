# Workspace

This example demonstrates how to uses `rules_buf` with [workspaces](https://docs.buf.build/reference/workspaces).

- Export each `buf.yaml` ([fooapis](fooapis/BUILD.bazel#L4), [barapis](barapis/BUILD.bazel#L4))
- `proto_library` rules need an additional argument `strip_import_prefix` ([foo/v1](fooapis/foo/v1/BUILD.bazel#L7), [bar/v1](barapis/bar/v1/BUILD.bazel#L7))

Checkout the [gazelle example](../gazelle) for way to generate the rules.

# Examples

Examples on how to use `rules_buf` in various scenarios. For more info refer to the [official docs](https://docs.buf.build/build-systems/bazel).

## Scenarios

### [bzlmod](bzlmod)

This demonstrates using this repo with [bzlmod](https://docs.bazel.build/versions/5.0.0/bzlmod.html).

### [Version](version)

This demonstrates basic setup and pinning a `buf` cli version. This also demonstrates using the `buf` cli as a bazel toolchain.

### [Gazelle](gazelle)

This demonstrates setting up the `gazelle` plugin to generate `proto_library`, `buf_lint_test`, and `buf_breaking_test` rules.

### [Single Module](single_module)

This demonstrates setting up lint and breaking tests in a project with a `buf.yaml`.

### [Workspaces](workspace)

This demonstrates setting up lint and breaking tests in a [buf workspace](https://docs.buf.build/reference/workspaces) project.

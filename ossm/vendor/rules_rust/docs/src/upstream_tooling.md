# Upstream Tooling

rules_rust manages versions of things like rustc. If you want to manually run upstream tooling configured at the versions, plugins and such that rules_rust has configured, rules_rust exposes these as targets in `@rules_rust//tools/upstream_wrapper`:

```console
% bazel query @rules_rust//tools/upstream_wrapper
@rules_rust//tools/upstream_wrapper:cargo
@rules_rust//tools/upstream_wrapper:cargo_clippy
@rules_rust//tools/upstream_wrapper:rustc
@rules_rust//tools/upstream_wrapper:rustfmt
```

You can run them via `bazel run`, e.g. `bazel run @rules_rust//tools/upstream_wrapper:cargo -- check`.

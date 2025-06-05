# upstream_wrapper

Wrap the binaries from the current toolchain so that
they can be easily invoked with `bazel run`:

```bash
bazel run @rules_rust//tools/upstream_wrapper:cargo
bazel run @rules_rust//tools/upstream_wrapper:cargo -- clippy
bazel run @rules_rust//tools/upstream_wrapper:cargo -- fmt
bazel run @rules_rust//tools/upstream_wrapper:rustc -- main.rs
bazel run @rules_rust//tools/upstream_wrapper:rustfmt
```

Alternatively, look at the [bazel_env example](../../examples/bazel_env/)
to include them in the users path with direnv.

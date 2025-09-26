## Automatic updates

A convenient way to automatically keep your vendored copy up-to-date is to use the `write_aspect_bazelrc_presets` rule in `.aspect/bazelrc/BUILD.bazel`:

```python title=".aspect/bazelrc/BUILD.bazel"
"Aspect bazelrc presets; see https://docs.aspect.build/guides/bazelrc"

load("@aspect_bazel_lib//lib:bazelrc_presets.bzl", "write_aspect_bazelrc_presets")

write_aspect_bazelrc_presets(name = "update_aspect_bazelrc_presets")
```

When `@aspect_bazel_lib` is upgraded in your `WORKSPACE.bazel` or your `MODULE.bazel` file, a `diff_test`
stamped out by `write_aspect_bazelrc_presets` will fail if your vendored copy is out-of-date and print the Bazel command
to run to update it. For example, `bazel run //.aspect/bazelrc:update_aspect_bazelrc_presets`.

See the [bazelrc](https://github.com/aspect-build/bazel-examples/blob/main/bazelrc/.aspect/bazelrc/BUILD.bazel) example
in our [bazel-examples](https://github.com/aspect-build/bazel-examples) repository for a working example.

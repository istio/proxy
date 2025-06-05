# Aspect bazelrc presets

The `.bazelrc` files found here are the source-of-truth for our recommended Bazel presets.

They are mirrored on our docsite at https://docs.aspect.build/guides/bazelrc.

## Using Aspect bazelrc presets in your project

The `.bazelrc` file can get large, fast.
Some settings don't apply everywhere - some options are appropriate only on CI,
and some vary depending on the version of Bazel you use or languages used.

Bazel rc files can contain `import` statements, which allow you to organize the content better.

To use these presets in your project, simply vendor the `*.bazelrc` files from
https://github.com/bazel-contrib/bazel-lib/tree/main/.aspect/bazelrc into the
`.aspect/bazelrc` folder in your repository and `import` them in your `.bazelrc` file.

For example,

```python title=".bazelrc"
# Import Aspect bazelrc presets
import %workspace%/.aspect/bazelrc/bazel6.bazelrc
import %workspace%/.aspect/bazelrc/convenience.bazelrc
import %workspace%/.aspect/bazelrc/correctness.bazelrc
import %workspace%/.aspect/bazelrc/debug.bazelrc
import %workspace%/.aspect/bazelrc/javascript.bazelrc
import %workspace%/.aspect/bazelrc/performance.bazelrc

### YOUR PROJECT SPECIFIC OPTIONS GO HERE ###

# Load any settings & overrides specific to the current user from `.aspect/bazelrc/user.bazelrc`.
# This file should appear in `.gitignore` so that settings are not shared with team members. This
# should be last statement in this config so the user configuration is able to overwrite flags from
# this file. See https://bazel.build/configure/best-practices#bazelrc-file.
try-import %workspace%/.aspect/bazelrc/user.bazelrc
```

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

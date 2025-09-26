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

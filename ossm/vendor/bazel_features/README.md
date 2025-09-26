# Bazel Features

Use this to determine the availability of a Bazel feature in your ruleset. It works under the hood by comparing the Bazel version against a known range in which the feature is available. Example usage:

```starlark
load("@bazel_features//:features.bzl", "bazel_features")
if bazel_features.toolchains.has_optional_toolchains:
    # Do something
```

The [`features.bzl`](features.bzl) file contains the list of features.

### Accessing globals

References to global Starlark symbols that do not exist cause load time errors, which means that their availability in Bazel cannot be tested via a regular feature.
Instead, use `bazel_features.globals.<symbol>`, which is `<symbol>` if the symbol is available and `None` else.

See [`globals.bzl`](private/globals.bzl) for the list of symbols that can be checked for in this way.

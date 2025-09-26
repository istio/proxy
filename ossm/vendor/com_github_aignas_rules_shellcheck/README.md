# Shellcheck rules for bazel

Now you do not need to depend on the system `shellcheck` version in your bazel-managed (mono)repos.

[![Build Status](https://github.com/aignas/rules_shellcheck/workflows/CI/badge.svg)](https://github.com/aignas/rules_shellcheck/actions)

Choose your release from the [GH Releases](https://github.com/aignas/rules_shellcheck/releases) and follow setup instructions there.

Then `shellcheck` can be accessed by running:

```shell
bazel run @rules_shellcheck//:shellcheck -- <parameters>
```

And you can define a lint target:

```starlark
load("@rules_shellcheck//:def.bzl", "shellcheck", "shellcheck_test")

shellcheck_test(
    name = "shellcheck_test",
    data = glob(["*.sh"]),
    tags = ["lint"],
    format = "gcc",
    severity = "warning",
)
```

Note: this is a simple project that allows me to learn about various bazel concepts. Feel free to create PRs contributing to the project or consider using [rules_lint].

[rules_lint]: https://github.com/aspect-build/rules_lint

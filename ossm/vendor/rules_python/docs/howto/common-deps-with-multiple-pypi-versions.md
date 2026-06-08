(common-deps-with-multiple-pypi-versions)=
# How to use a common set of dependencies with multiple PyPI versions

In this guide, we show how to handle a situation common to monorepos
that extensively share code: How does a common library refer to the correct
`@pypi_<name>` hub when binaries may have their own requirements (and thus
PyPI hub name)? Stated as code, this situation:

```bzl

py_binary(
  name = "bin_alpha",
  deps = ["@pypi_alpha//requests", ":common"],
)
py_binary(
  name = "bin_beta",
  deps = ["@pypi_beta//requests", ":common"],
)

py_library(
  name = "common",
  deps = ["@pypi_???//more_itertools"] # <-- Which @pypi repo?
)
```

## Using flags to pick a hub

The basic trick to make `:common` pick the appropriate `@pypi_<name>` is to use
`select()` to choose one based on build flags. To help this process, `py_binary`
et al allow forcing particular build flags to be used, and custom flags can be
registered to allow `py_binary` et al to set them.

In this example, we create a custom string flag named `//:pypi_hub`,
register it to allow using it with `py_binary` directly, then use `select()`
to pick different dependencies.

```bzl
# File: MODULE.bazel

rules_python_config.add_transition_setting(
    setting = "//:pypi_hub",
)

# File: BUILD.bazel

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

string_flag(
    name = "pypi_hub",
)

config_setting(
    name = "is_pypi_alpha",
    flag_values = {"//:pypi_hub": "alpha"},
)

config_setting(
    name = "is_pypi_beta",
    flag_values = {"//:pypi_hub": "beta"}
)

py_binary(
    name = "bin_alpha",
    srcs = ["bin_alpha.py"],
    config_settings = {
        "//:pypi_hub": "alpha",
    },
    deps = ["@pypi_alpha//requests", ":common"],
)
py_binary(
    name = "bin_beta",
    srcs = ["bin_beta.py"],
    config_settings = {
        "//:pypi_hub": "beta",
    },
    deps = ["@pypi_beta//requests", ":common"],
)
py_library(
    name = "common",
    deps = select({
        ":is_pypi_alpha": ["@pypi_alpha//more_itertools"],
        ":is_pypi_beta": ["@pypi_beta//more_itertools"],
    }),
)
```

When `bin_alpha` and `bin_beta` are built, they will have the `pypi_hub`
flag force to their respective value. When `:common` is evaluated, it sees
the flag value of the binary that is consuming it, and the `select()` resolves
appropriately.

This rule is to make it easier to generate `console_script` entry points
as per Python [specification].

Generate a `py_binary` target for a particular console_script `entry_point`
from a PyPI package, e.g. for creating an executable `pylint` target use:
```starlark
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

py_console_script_binary(
    name = "pylint",
    pkg = "@pip//pylint",
)
```

#### Specifying extra dependencies 
You can also specify extra dependencies and the
exact script name you want to call. It is useful for tools like `flake8`, `pylint`,
`pytest`, which have plugin discovery methods and discover dependencies from the
PyPI packages available in the `PYTHONPATH`.
```starlark
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

py_console_script_binary(
    name = "pylint_with_deps",
    pkg = "@pip//pylint",
    # Because `pylint` has multiple console_scripts available, we have to
    # specify which we want if the name of the target name 'pylint_with_deps'
    # cannot be used to guess the entry_point script.
    script = "pylint",
    deps = [
        # One can add extra dependencies to the entry point.
        # This specifically allows us to add plugins to pylint.
        "@pip//pylint_print",
    ],
)
```

#### Using a specific Python version

A specific Python version can be forced by passing the desired Python version, e.g. to force Python 3.9:
```starlark
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

py_console_script_binary(
    name = "yamllint",
    pkg = "@pip//yamllint",
    python_version = "3.9"
)
```

#### Using a specific Python Version directly from a Toolchain
:::{deprecated} 1.1.0
The toolchain specific `py_binary` and `py_test` symbols are aliases to the regular rules. 
i.e. Deprecated `load("@python_versions//3.11:defs.bzl", "py_binary")` and `load("@python_versions//3.11:defs.bzl", "py_test")`

You should instead specify the desired python version with `python_version`; see above example.
:::
Alternatively, the [`py_console_script_binary.binary_rule`] arg can be passed
the version-bound `py_binary` symbol, or any other `py_binary`-compatible rule
of your choosing:
```starlark
load("@python_versions//3.9:defs.bzl", "py_binary")
load("@rules_python//python/entry_points:py_console_script_binary.bzl", "py_console_script_binary")

py_console_script_binary(
    name = "yamllint",
    pkg = "@pip//yamllint:pkg",
    binary_rule = py_binary,
)
```

[specification]: https://packaging.python.org/en/latest/specifications/entry-points/
[`py_console_script_binary.binary_rule`]: #py_console_script_binary_binary_rule
# pip_parse vendored

This example is like pip_parse, however we avoid loading from the generated file.
See https://github.com/bazel-contrib/rules_python/issues/608
and https://blog.aspect.dev/avoid-eager-fetches.

The requirements now form a triple:

- requirements.in - human editable, expresses only direct dependencies and load-bearing version constraints
- requirements.txt - lockfile produced by pip-compile or other means
- requirements.bzl - the "parsed" version of the lockfile readable by Bazel downloader

The `requirements.bzl` file contains baked-in attributes such as `python_interpreter_target` as they were specified in the original `pip_parse` rule. These can be overridden at install time by passing arguments to `install_deps`. For example: 

```python
# Register a hermetic toolchain
load("@rules_python//python:repositories.bzl", "python_register_toolchains")

python_register_toolchains(
    name = "python39",
    python_version = "3.9",
)

# Load dependencies vendored by some other ruleset.
load("@some_rules//:py_deps.bzl", "install_deps")

install_deps(
    python_interpreter_target = "@python39_host//:python",
)
```

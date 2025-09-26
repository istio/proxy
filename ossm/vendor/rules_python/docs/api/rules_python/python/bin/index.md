:::{default-domain} bzl
:::
:::{bzl:currentfile} //python/bin:BUILD.bazel
:::

# //python/bin

:::{bzl:target} python

A target to directly run a Python interpreter.

By default, it uses the Python version that toolchain resolution matches
(typically the one set with `python.defaults(python_version = ...)` in
`MODULE.bazel`).

This runs a Python interpreter in a similar manner as when running `python3`
on the command line. It can be invoked using `bazel run`. Remember that in
order to pass flags onto the program `--` must be specified to separate
Bazel flags from the program flags.

An example that will run Python 3.12 and have it print the version

```
bazel run @rules_python//python/bin:python \
  `--@rule_python//python/config_settings:python_verion=3.12 \
  -- \
  --version
```

::::{seealso}
The {flag}`--python_src` flag for using the intepreter a binary/test uses.
::::

::::{versionadded} 1.3.0
::::
:::

:::{bzl:flag} python_src

The target (one providing `PyRuntimeInfo`) whose python interpreter to use for
{obj}`:python`.
:::

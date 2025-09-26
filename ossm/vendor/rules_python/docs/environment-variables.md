# Environment Variables

::::{envvar} RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS

This variable allows for additional arguments to be provided to the Python interpreter
at bootstrap time when the `bash` bootstrap is used. If
`RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS` were provided as `-Xaaa`, then the command
would be:

```
python -Xaaa /path/to/file.py
```

This feature is likely to be useful for the integration of debuggers. For example,
it would be possible to configure `RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS` to
be set to `/path/to/debugger.py --port 12344 --file`, resulting
in the command executed being:

```
python /path/to/debugger.py --port 12345 --file /path/to/file.py
```

:::{seealso}
The {bzl:obj}`interpreter_args` attribute.
:::

:::{versionadded} 1.3.0

::::

:::{envvar} RULES_PYTHON_BOOTSTRAP_VERBOSE

When `1`, debug information about bootstrapping of a program is printed to
stderr.
:::

:::{envvar} RULES_PYTHON_BZLMOD_DEBUG

When `1`, bzlmod extensions will print debug information about what they're
doing. This is mostly useful for development to debug errors.
:::

:::{envvar} RULES_PYTHON_DEPRECATION_WARNINGS

When `1`, `rules_python` will warn users about deprecated functionality that will
be removed in a subsequent major `rules_python` version. Defaults to `0` if unset.
:::

::::{envvar} RULES_PYTHON_ENABLE_PYSTAR

When `1`, the `rules_python` Starlark implementation of the core rules is used
instead of the Bazel-builtin rules. Note that this requires Bazel 7+. Defaults
to `1`.

:::{versionadded} 0.26.0
Defaults to `0` if unspecified.
:::
:::{versionchanged} 0.40.0
The default became `1` if unspecified
:::
::::

::::{envvar} RULES_PYTHON_ENABLE_PIPSTAR

When `1`, the `rules_python` Starlark implementation of the PyPI/pip integration is used
instead of the legacy Python scripts.

:::{versionadded} 1.5.0
:::
::::

::::{envvar} RULES_PYTHON_EXTRACT_ROOT

Directory to use as the root for creating files necessary for bootstrapping so
that a binary can run.

Only applicable when {bzl:flag}`--venvs_use_declare_symlink=no` is used.

When set, a binary will attempt to find a unique, reusable, location within this
directory for the files it needs to create to aid startup. The files may not be
deleted upon program exit; it is the responsibility of the caller to ensure
cleanup.

Manually specifying the directory is useful to lower the overhead of
extracting/creating files on every program execution. By using a location
outside /tmp, longer lived programs don't have to worry about files in /tmp
being cleaned up by the OS.

If not set, then a temporary directory will be created and deleted upon program
exit.

:::{versionadded} 1.2.0
:::
::::

:::{envvar} RULES_PYTHON_GAZELLE_VERBOSE

When `1`, debug information from Gazelle is printed to stderr.
::::

:::{envvar} RULES_PYTHON_PIP_ISOLATED

Determines if `--isolated` is used with pip.

Valid values:
* `0` and `false` mean to not use isolated mode
* Other non-empty values mean to use isolated mode.
:::

:::{envvar} RULES_PYTHON_REPO_DEBUG

When `1`, repository rules will print debug information about what they're
doing. This is mostly useful for development to debug errors.
:::

:::{envvar} RULES_PYTHON_REPO_DEBUG_VERBOSITY

Determines the verbosity of logging output for repo rules. Valid values:

* `DEBUG`
* `FAIL`
* `INFO`
* `TRACE`
:::

:::{envvar} RULES_PYTHON_REPO_TOOLCHAIN_VERSION_OS_ARCH

Determines the Python interpreter platform to be used for a particular
interpreter `(version, os, arch)` triple to be used in repository rules.
Replace the `VERSION_OS_ARCH` part with actual values when using, e.g.,
`3_13_0_linux_x86_64`. The version values must have `_` instead of `.` and the
os, arch values are the same as the ones mentioned in the
`//python:versions.bzl` file.
:::

:::{envvar} VERBOSE_COVERAGE

When `1`, debug information about coverage behavior is printed to stderr.
:::

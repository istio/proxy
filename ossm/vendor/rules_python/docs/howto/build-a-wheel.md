:::{default-domain} bzl
:::

# How to build a wheel

This guide explains how to use the `py_wheel` rule to build a wheel
file from a `py_library`.

## Basic usage

The `py_wheel` rule takes any file-providing target as input and put its files
into a wheel. Because `py_library` provides its source files, simple cases can
pass `py_library` directly to `py_wheel`:

```starlark
# BUILD.bazel

py_library(
    name = "my_project_lib",
    srcs = glob(["my_project/**/*.py"]),
    # ...
)

py_wheel(
    name = "my_project_wheel",
    distribution = "my-project",
    version = "0.1.0",
    deps = [":my_project_lib"],
)
```

The above will include the *default outputs* of the `py_library`, which are the
direct `.py` files listed in the py library. It does **not** include transitive
dependencies.

## Including and filtering transitive dependencies


Use the `py_package` rule to include and filter the transitive parts of
a `py_library` target.

The `py_package` rule has a `packages` attribute that takes a list of dotted
Python package names to include. All files and dependencies of those packages
are included.

Here is an example:

```starlark
# BUILD.bazel

py_library(
    name = "my_project_lib",
    srcs = glob(["my_project/**/*.py"]),
    deps = ["@pypi//some_dep"],
)

py_package(
    name = "my_project_package",
    # This will only include files for the "my_package" package; other files
    # will be excluded.
    packages = ["my_project"],
)

py_wheel(
    name = "my_project_wheel",
    distribution = "my-project",
    version = "0.1.0",
    # The `py_wheel` rule takes the `py_package` target in the `deps`
    # attribute.
    deps = [":my_project_package"],
)
```

## Disabling `__init__.py` generation

By default, Bazel automatically creates `__init__.py` files in directories to
make them importable. This can sometimes be undesirable when building wheels
because it interfers with namespace packages or makes directories importable
that shouldn't be importable.

It's highly recommended to disable this behavior by setting a flag in your
`.bazelrc` file:

```
# .bazelrc
build --incompatible_default_to_explicit_init_py=true
```
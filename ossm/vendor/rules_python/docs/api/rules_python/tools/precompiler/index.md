:::{bzl:currentfile} //tools/precompiler:BUILD.bazel
:::

# //tools/precompiler

:::{bzl:flag} execution_requirements
Determines the execution requirements `//tools/precompiler:precompiler` uses.

This is a repeatable string_list flag. The values are `key=value` entries, each
of which are added to the execution requirements for the `PyCompile` action to
generate pyc files.

Customizing this flag mostly allows controlling whether Bazel runs the
precompiler as a regular worker, persistent worker, or regular action.
:::

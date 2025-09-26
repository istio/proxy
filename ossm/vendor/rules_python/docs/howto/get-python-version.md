:::{default-domain} bzl
:::

# How to get the current Python version

This guide explains how to use a [toolchain](toolchains) to get the current Python
version and, as an example, write it to a file.

You can create a simple rule that accesses the Python toolchain and retrieves
the version string.

## The rule implementation

Create a file named `my_rule.bzl`:

```starlark
# my_rule.bzl
def _my_rule_impl(ctx):
    toolchain = ctx.toolchains["@rules_python//python:toolchain_type"]
    info = toolchain.py3_runtime.interpreter_version_info
    python_version = str(info.major) + "." + str(info.minor) + "." + str(info.micro)

    output_file = ctx.actions.declare_file(ctx.attr.name + ".txt")
    ctx.actions.write(
        output = output_file,
        content = python_version,
    )

    return [DefaultInfo(files = depset([output_file]))]

my_rule = rule(
    implementation = _my_rule_impl,
    attrs = {},
    toolchains = ["@rules_python//python:toolchain_type"],
)
```

The `info` variable above is a {obj}`PyRuntimeInfo` object, which contains
information about the Python runtime. It contains more than just the version;
see the {obj}`PyRuntimeInfo` docs for its API documentation.

## Using the rule

In your `BUILD.bazel` file, you can use the rule like this:

```starlark
# BUILD.bazel
load(":my_rule.bzl", "my_rule")

my_rule(
    name = "show_python_version",
)
```

When you build this target, it will generate a file named
`show_python_version.txt` containing the Python version (e.g., `3.9`).

```starlark
bazel build :show_python_version
cat bazel-bin/show_python_version.txt
```

:::{default-domain} bzl
:::

# How to link to libpython

This guide explains how to use the Python [toolchain](toolchains) to get the linker
flags required for linking against `libpython`. This is often necessary when
embedding Python in a C/C++ application.

Currently, the `:current_py_cc_libs` target does *not* include `-lpython` et al
linker flags. This is intentional because it forces dynamic linking (via the
dynamic linker processing `DT_NEEDED` entries), which prevents users who want
to load it in some more custom way.

## Exposing linker flags in a rule

You can create a rule that gets the Python version from the toolchain and
constructs the correct linker flag. This rule can then provide the flag to
other C/C++ rules via the `CcInfo` provider.

Here's an example of a rule that creates the `-lpython<version>` flag:

```starlark
# python_libs.bzl
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "cc_common")

def _python_libs_impl(ctx):
    toolchain = ctx.toolchains["@rules_python//python:toolchain_type"]
    info = toolchain.py3_runtime.interpreter_version_info
    link_flag = "-lpython{}.{}".format(info.major, info.minor)

    cc_info = CcInfo(
        linking_context = cc_common.create_linking_context(
            user_link_flags = [link_flag],
        ),
    )
    return [cc_info]

python_libs = rule(
    implementation = _python_libs_impl,
    toolchains = ["@rules_python//python:toolchain_type"],
)
```

## Using the rule

In your `BUILD.bazel` file, define a target using this rule and add it to the
`deps` of your `cc_binary` or `cc_library`.

```starlark
# BUILD.bazel
load(":python_libs.bzl", "python_libs")

python_libs(
    name = "py_libs",
)

cc_binary(
    name = "my_app",
    srcs = ["my_app.c"],
    deps = [
        ":py_libs",
        # Other dependencies
    ],
)
```

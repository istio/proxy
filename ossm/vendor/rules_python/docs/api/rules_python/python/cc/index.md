:::{default-domain} bzl
:::
:::{bzl:currentfile} //python/cc:BUILD.bazel
:::
# //python/cc

:::{bzl:target} current_py_cc_headers

A convenience target that provides the Python headers. It uses toolchain
resolution to find the headers for the Python runtime matching the interpreter
that will be used. This basically forwards the underlying
`cc_library(name="python_headers")` target defined in the `@python_X_Y` repo.

This target provides:

* `CcInfo`: The C++ information about the Python headers.
:::

:::{bzl:target} current_py_cc_libs

A convenience target that provides the Python libraries. It uses toolchain
resolution to find the libraries for the Python runtime matching the interpreter
that will be used. This basically forwards the underlying
`cc_library(name="libpython")` target defined in the `@python_X_Y` repo.

This target provides:

* `CcInfo`: The C++ information about the Python libraries.
:::

:::{bzl:target} toolchain_type

Toolchain type identifier for the Python C toolchain.

This toolchain type is typically implemented by {obj}`py_cc_toolchain`.

::::{seealso}
{any}`Custom Toolchains` for how to define custom toolchains
::::

:::

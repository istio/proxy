:::{default-domain} bzl
:::

# How to get Python headers for C extensions

When building a Python C extension, you need access to the Python header
files. This guide shows how to get the necessary include paths from the Python
[toolchain](toolchains).

The recommended way to get the headers is to depend on the
{obj}`@rules_python//python/cc:current_py_cc_headers` or
{obj}`@rules_python//python/cc:current_py_cc_headers_abi3`
targets. These are convenience targets that use toolchain resolution to find
the correct headers for the target platform.

## Using the headers

In your `BUILD.bazel` file, you can add `@rules_python//python/cc:current_py_cc_headers`
to the `deps` of a `cc_library` or `cc_binary` target.

```bazel
# BUILD.bazel
cc_library(
    name = "my_c_extension",
    srcs = ["my_c_extension.c"],
    deps = ["@rules_python//python/cc:current_py_cc_headers"],
)
```

This setup ensures that your C extension code can find and use the Python
headers during compilation.

:::{note}
The `:current_py_cc_headers` target provides all the Python headers. This _may_
include ABI-specific information.
:::

## Using the stable ABI headers

If you're building for the [Python stable ABI](https://docs.python.org/3/c-api/stable.html),
then depend on {obj}`@rules_python//python/cc:current_py_cc_headers_abi3`. This
target contains only objects relevant to the Python stable ABI. Remember to
define
[`Py_LIMITED_API`](https://docs.python.org/3/c-api/stable.html#c.Py_LIMITED_API)
when building such extensions.

```bazel
# BUILD.bazel
cc_library(
    name = "my_stable_abi_extension",
    srcs = ["my_stable_abi_extension.c"],
    deps = ["@rules_python//python/cc:current_py_cc_headers_abi3"],
)
```

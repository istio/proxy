:::{default-domain} bzl
:::

# How to get Python headers for C extensions

When building a Python C extension, you need access to the Python header
files. This guide shows how to get the necessary include paths from the Python
[toolchain](toolchains).

The recommended way to get the headers is to depend on the
`@rules_python//python/cc:current_py_cc_headers` target. This is a helper
target that uses toolchain resolution to find the correct headers for the
target platform.

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
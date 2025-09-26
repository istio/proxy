:::{default-domain} bzl
:::

# How to expose headers from a PyPI package

When you depend on a PyPI package that includes C headers (like `numpy`), you
need to make those headers available to your `cc_library` or
`cc_binary` targets.

The recommended way to do this is to inject a `BUILD.bazel` file into the
external repository for the package. This `BUILD` file will create
a `cc_library` target that exposes the header files.

First, create a `.bzl` file that has the extra logic we'll inject. Putting it
in a separate bzl file avoids having to redownload and extract the whl file
when our logic changes.

```bzl

# pypi_extra_targets.bzl
load("@rules_cc//cc:cc_library.bzl", "cc_library")

def extra_numpy_targets():
    cc_library(
        name = "headers",
        hdrs = glob(["**/*.h"]),
        visibility = ["//visibility:public"],
    )
```

## Bzlmod setup

In your `MODULE.bazel` file, use the `build_file_content` attribute of
`pip.parse` to inject the `BUILD` file content for the `numpy` package.

```bazel
# MODULE.bazel
load("@rules_python//python/extensions:pip.bzl", "parse", "whl_mods")
pip = use_extension("@rules_python//python/extensions:pip.bzl", "pip")
whl_mods = use_extension("@rules_python//python/extensions:pip.bzl", "whl_mods")


# Define a specific modification for a wheel
whl_mods(
    hub_name = "pypi_mods",
    whl_name = "numpy-1.0.0-py3-none-any.whl", # The exact wheel filename
    additive_build_content = """
load("@//:pypi_extra_targets.bzl", "numpy_hdrs")

extra_numpy_targets()
""",
)
pip.parse(
    hub_name = "pypi",
    wheel_name = "numpy",
    requirements_lock = "//:requirements.txt",
    whl_modifications = {
        "@pypi_mods//:numpy.json": "numpy",
    },
    extra_hub_aliases = {
        "numpy": ["headers"],
    }
)
```

## WORKSPACE setup

In your `WORKSPACE` file, use the `annotations` attribute of `pip_parse` to
inject additional `BUILD` file content, then use `extra_hub_targets` to expose
that target in the `@pypi` hub repo.

The {obj}`package_annotation` helper can be used to construct the value for the
`annotations` attribute.

```starlark
# WORKSPACE
load("@rules_python//python:pip.bzl", "package_annotation", "pip_parse")

pip_parse(
    name = "pypi",
    requirements_lock = "//:requirements.txt",
    annotations = {
        "numpy": package_annotation(
            additive_build_content = """\
load("@//:pypi_extra_targets.bzl", "numpy_hdrs")

extra_numpy_targets()
"""
        ),
    },
    extra_hub_targets = {
        "numpy": ["headers"],
    },
)
```

## Using the headers

In your `BUILD.bazel` file, you can now depend on the generated `headers`
target.

```bazel
# BUILD.bazel
cc_library(
    name = "my_c_extension",
    srcs = ["my_c_extension.c"],
    deps = ["@pypi//numpy:headers"],
)
```

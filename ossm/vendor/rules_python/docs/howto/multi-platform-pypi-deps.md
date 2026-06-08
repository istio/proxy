:::{default-domain} bzl
:::

# How-to: Multi-Platform PyPI Dependencies

When developing applications that need to run on a wide variety of platforms,
managing PyPI dependencies can become complex. You might need different sets of
dependencies for different combinations of Python version, threading model,
operating system, CPU architecture, libc, and even hardware accelerators like
GPUs.

This guide demonstrates how to manage this complexity using `rules_python` with
bzlmod. If you prefer to learn by example, complete example code is provided at
the end.

In this how to guide, we configure for using 4 requirements files, each
for a different variation using Python 3.14 on Linux:

* Regular (non-freethreaded) Python
* Freethreaded Python
* Regular Python for CUDA 12.9
* Freethreaded Python for ARM and Musl

## Mapping requirements files to Bazel configuration settings

Unfortunately, a requirements file doesn't tell what it's compatible with,
so we have to manually specify the Bazel configuration settings for it. To do
that using rules_python, there are two steps: defining a platform, then
associating a requirements file with the platform.

### Defining a platform

First, we define a "platform" using {obj}`pip.default`. This associates an
arbitrary name with a list of Bazel {obj}`config_setting` targets. While any
name can be used for a platform (its name has no inherent semantic meaning), it
should encode all the relevant dimensions that distinguish a requirements file.
For example, if a requirements file is specifically for the combination of CUDA
12.9 and NumPy 2.0, then the platform name should represent that.

The convention is to follow the format of `{os}_{cpu}{threading}`, where:

* `{os}` is the operating system (`linux`, `osx`, `windows`).
* `{cpu}` is the architecture (`x86_64`, `aarch64`).
* `{threading}` is `_freethreaded` for a freethreaded Python runtime, or an
  empty string for the regular runtime.

Additional dimensions should be appended and separated with an underscore (e.g.
`linux_x86_64_musl_cuda12.9_numpy2`).

The platform name should not include the Python version. That is handled by
{attr}`pip.parse.python_version` separately.

:::{note}
The term _platform_ here has nothing to do with Bazel's `platform()` rule.
:::

#### Defining custom settings

Because {attr}`pip.default.config_settings` is a list of arbitrary `config_setting`
targets, you can define your own flags or implement custom config matching
logic. This allows you to model settings that aren't inherently part of
rules_python.

This is typically done using [bazel_skylib flags](https://bazel.build/extending/config), but any [Starlark
defined build setting](https://bazel.build/extending/config) can be used. Just
remember to use `config_setting()` to match a particular value of the flag.

In our example below, we define a custom flag for CUDA version.

#### Predefined and common build settings

rules_python has some predefined build settings you can use. Commonly used ones
are:

* {obj}`@rules_python//python/config_settings:py_linux_libc`
* {obj}`@rules_python//python/config_settings:py_freethreaded`

Additionally, [Bazel @platforms](https://github.com/bazelbuild/platforms)
contains commonly used settings for OS and CPU:

* `@platforms//os:windows`
* `@platforms//os:linux`
* `@platforms//os:osx`
* `@platforms//cpu:x86_64`
* `@platforms//cpu:aarch64`

Note that these are the raw flag names. In order to use them with `pip.default`,
you must use {obj}`config_setting` to match a particular value for them.

### Associating Requirements to Platforms

Next, we associate a requirements file with a platform using
{obj}`pip.parse.requirements_by_platform`. This is a dictionary attribute where
the keys are requirements files and the value is a platform name. The platform
value can use a trailing or leading `*` to match multiple platforms. It can also
specify multiple platform names using commas to separate them.

Note that the Python version is _not_ part of the platform name.

Under the hood, `pip.parse` merges all the requirements (for a `hub_name`) and
constructs `select()` expressions to route to the appropriate dependencies.

### Using it in practice

Finally, to make use of what we've configured, perform a build and set
command line flags to the appropriate values.

```shell
# Build for CUDA
bazel build --//:cuda_version=12.9 //:binary

# Build for ARM with musl
bazel build --@rules_python//python/config_settings:py_linux_libc=musl \
  --cpu=aarch64 //:binary

# Build for freethreaded
bazel build --@rules_python//python/config_settings:py_freethreaded=yes //:binary
```

Note that certain combinations of flags may result in an error or undefined
behavior. For example, trying to set both freethreaded and CUDA at the same
time would result in an error because no requirements file was registered
to match that combination.

## Multiple Python Versions

Having multiple Python versions is fully supported. Simply add a `pip.parse()`
call and set `python_version` appropriately.

## Multiple hubs

Having multiple `pip.parse` calls with different `hub_name` values is fully
supported. Each hub only contains the requirements registered to it.

## Complete Example

Here is a complete example that puts all the pieces together.

```starlark
# File: BUILD.bazel
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

# A custom flag for controlling the CUDA version
string_flag(
    name = "cuda_version",
    build_setting_default = "none",
)

config_setting(
    name = "is_cuda_12_9",
    flag_values = {":cuda_version": "12.9"},
)

# A config_setting that uses the built-in libc flag from rules_python
config_setting(
    name = "is_musl",
    flag_values = {"@rules_python//python/config_settings:py_linux_libc": "muslc"},
)

# File: MODULE.bazel
pip = use_extension("@rules_python//python/extensions:pip.bzl", "pip")

# A custom platform for CUDA on glibc linux
pip.default(
    platform = "linux_x86_64_cuda12.9",
    arch_name = "x86_64",
    os_name = "linux",
    config_settings = ["@//:is_cuda_12_9"],
)

# A custom platform for musl on linux
pip.default(
    platform = "linux_aarch64_musl",
    os_name = "linux",
    arch_name = "aarch64",
    config_settings = ["@//:is_musl"],
)

pip.parse(
    hub_name = "my_deps",
    python_version = "3.14",
    requirements_by_platform = {
        # Map to default platform names
        "//:py3.14-regular-linux-x86-glibc-cpu.txt": "linux_x86_64",
        "//:py3.14-freethreaded-linux-x86-glibc-cpu.txt": "linux_x86_64_freethreaded",

        # Map to our custom platform names
        "//:py3.14-regular-linux-x86-glibc-cuda12.9.txt": "linux_x86_64_cuda12.9",
        "//:py3.14-freethreaded-linux-arm-musl-cpu.txt": "linux_aarch64_musl",
    },
)

use_repo(pip, "my_deps")
```

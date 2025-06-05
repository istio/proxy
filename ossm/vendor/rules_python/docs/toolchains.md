:::{default-domain} bzl
:::

# Configuring Python toolchains and runtimes

This documents how to configure the Python toolchain and runtimes for different
use cases.

## Bzlmod MODULE configuration

How to configure `rules_python` in your MODULE.bazel file depends on how and why
you're using Python. There are 4 basic use cases:

1. A root module that always uses Python. For example, you're building a
   Python application.
2. A library module with dev-only uses of Python. For example, a Java project
   that only uses Python as part of testing itself.
3. A library module without version constraints. For example, a rule set with
   Python build tools, but defers to the user as to what Python version is used
   for the tools.
4. A library module with version constraints. For example, a rule set with
   Python build tools, and the module requires a specific version of Python
   be used with its tools.

### Root modules

Root modules are always the top-most module. These are special in two ways:

1. Some `rules_python` bzlmod APIs are only respected by the root module.
2. The root module can force module overrides and specific module dependency
   ordering.

When configuring `rules_python` for a root module, you typically want to
explicitly specify the Python version you want to use. This ensures that
dependencies don't change the Python version out from under you. Remember that
`rules_python` will set a version by default, but it will change regularly as
it tracks a recent Python version.

NOTE: If your root module only uses Python for development of the module itself,
you should read the dev-only library module section.

```
bazel_dep(name="rules_python", version=...)
python = use_extension("@rules_python//python/extensions:python.bzl", "python")

python.toolchain(python_version = "3.12", is_default = True)
```

### Library modules

A library module is a module that can show up in arbitrary locations in the
bzlmod module graph -- it's unknown where in the breadth-first search order the
module will be relative to other modules. For example, `rules_python` is a
library module.

#### Library modules with dev-only Python usage

A library module with dev-only Python usage is usually one where Python is only
used as part of its tests. For example, a module for Java rules might run some
Python program to generate test data, but real usage of the rules don't need
Python to work. To configure this, follow the root-module setup, but remember to
specify `dev_dependency = True` to the bzlmod APIs:

```
# MODULE.bazel
bazel_dep(name = "rules_python", version=..., dev_dependency = True)

python = use_extension(
    "@rules_python//python/extensions:python.bzl",
    "python",
    dev_dependency = True
)

python.toolchain(python_version = "3.12", is_default=True)
```

#### Library modules without version constraints

A library module without version constraints is one where the version of Python
used for the Python programs it runs isn't chosen by the module itself. Instead,
it's up to the root module to pick an appropriate version of Python.

For this case, configuration is simple: just depend on `rules_python` and use
the normal `//python:py_binary.bzl` et al rules. There is no need to call
`python.toolchain` -- rules_python ensures _some_ Python version is available,
but more often the root module will specify some version.

```
# MODULE.bazel
bazel_dep(name = "rules_python", version=...)
```

#### Library modules with version constraints

A library module with version constraints is one where the module requires a
specific Python version be used with its tools. This has some pros/cons:

* It allows the library's tools to use a different version of Python than
  the rest of the build. For example, a user's program could use Python 3.12,
  while the library module's tools use Python 3.10.
* It reduces the support burden for the library module because the library only needs
  to test for the particular Python version they intend to run as.
* It raises the support burden for the library module because the version of
  Python being used needs to be regularly incremented.
* It has higher build overhead because additional runtimes and libraries need
  to be downloaded, and Bazel has to keep additional configuration state.

To configure this, request the Python versions needed in MODULE.bazel and use
the version-aware rules for `py_binary`.

```
# MODULE.bazel
bazel_dep(name = "rules_python", version=...)

python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(python_version = "3.12")

# BUILD.bazel
load("@rules_python//python:py_binary.bzl", "py_binary")

py_binary(..., python_version="3.12")
```

### Pinning to a Python version

Pinning to a version allows targets to force that a specific Python version is
used, even if the root module configures a different version as a default. This
is most useful for two cases:

1. For submodules to ensure they run with the appropriate Python version
2. To allow incremental, per-target, upgrading to newer Python versions,
   typically in a mono-repo situation.

To configure a submodule with the version-aware rules, request the particular
version you need when defining the toolchain:

```starlark
# MODULE.bazel
python = use_extension("@rules_python//python/extensions:python.bzl", "python")

python.toolchain(
    python_version = "3.11",
)
use_repo(python)
```

Then use the `@rules_python` repo in your BUILD file to explicity pin the Python version when calling the rule:

```starlark
# BUILD.bazel
load("@rules_python//python:py_binary.bzl", "py_binary")

py_binary(..., python_version = "3.11")
py_test(..., python_version = "3.11")
```

Multiple versions can be specified and used within a single build.

```starlark
# MODULE.bazel
python = use_extension("@rules_python//python/extensions:python.bzl", "python")

python.toolchain(
    python_version = "3.11",
    is_default = True,
)

python.toolchain(
    python_version = "3.12",
)

# BUILD.bazel
load("@rules_python//python:py_binary.bzl", "py_binary")
load("@rules_python//python:py_test.bzl", "py_test")

# Defaults to 3.11
py_binary(...)
py_test(...)

# Explicitly use Python 3.11
py_binary(..., python_version = "3.11")
py_test(..., python_version = "3.11")

# Explicitly use Python 3.12
py_binary(..., python_version = "3.12")
py_test(..., python_version = "3.12")
```

For more documentation, see the bzlmod examples under the {gh-path}`examples`
folder.  Look for the examples that contain a `MODULE.bazel` file.

### Other toolchain details

The `python.toolchain()` call makes its contents available under a repo named
`python_X_Y`, where X and Y are the major and minor versions. For example,
`python.toolchain(python_version="3.11")` creates the repo `@python_3_11`.
Remember to call `use_repo()` to make repos visible to your module:
`use_repo(python, "python_3_11")`


:::{deprecated} 1.1.0
The toolchain specific `py_binary` and `py_test` symbols are aliases to the regular rules. 
i.e. Deprecated `load("@python_versions//3.11:defs.bzl", "py_binary")` & `load("@python_versions//3.11:defs.bzl", "py_test")`

Usages of them should be changed to load the regular rules directly; 
i.e.  Use `load("@rules_python//python:py_binary.bzl", "py_binary")` & `load("@rules_python//python:py_test.bzl", "py_test")` and then specify the `python_version` when using the rules corresponding to the python version you defined in your toolchain. {ref}`Library modules with version constraints`
:::


#### Toolchain usage in other rules

Python toolchains can be utilized in other bazel rules, such as `genrule()`, by
adding the `toolchains=["@rules_python//python:current_py_toolchain"]`
attribute. You can obtain the path to the Python interpreter using the
`$(PYTHON2)` and `$(PYTHON3)` ["Make"
Variables](https://bazel.build/reference/be/make-variables). See the
{gh-path}`test_current_py_toolchain <tests/load_from_macro/BUILD.bazel>` target
for an example.

### Overriding toolchain defaults and adding more versions

One can perform various overrides for the registered toolchains from the root
module. For example, the following use cases would be supported using the
existing attributes:

* Limiting the available toolchains for the entire `bzlmod` transitive graph
  via {attr}`python.override.available_python_versions`.
* Setting particular `X.Y.Z` Python versions when modules request `X.Y` version
  via {attr}`python.override.minor_mapping`.
* Per-version control of the coverage tool used using
  {attr}`python.single_version_platform_override.coverage_tool`.
* Adding additional Python versions via {bzl:obj}`python.single_version_override` or
  {bzl:obj}`python.single_version_platform_override`.

### Using defined toolchains from WORKSPACE

It is possible to use toolchains defined in `MODULE.bazel` in `WORKSPACE`. For example
the following `MODULE.bazel` and `WORKSPACE` provides a working {bzl:obj}`pip_parse` setup:
```starlark
# File: WORKSPACE
load("@rules_python//python:repositories.bzl", "py_repositories")

py_repositories()

load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "third_party",
    requirements_lock = "//:requirements.txt",
    python_interpreter_target = "@python_3_10_host//:python",
)

load("@third_party//:requirements.bzl", "install_deps")

install_deps()

# File: MODULE.bazel
bazel_dep(name = "rules_python", version = "0.40.0")

python = use_extension("@rules_python//python/extensions:python.bzl", "python")

python.toolchain(is_default = True, python_version = "3.10")

use_repo(python, "python_3_10", "python_3_10_host")
```

Note, the user has to import the `*_host` repository to use the python interpreter in the
{bzl:obj}`pip_parse` and {bzl:obj}`whl_library` repository rules and once that is done
users should be able to ensure the setting of the default toolchain even during the
transition period when some of the code is still defined in `WORKSPACE`.

## Workspace configuration

To import rules_python in your project, you first need to add it to your
`WORKSPACE` file, using the snippet provided in the
[release you choose](https://github.com/bazelbuild/rules_python/releases)

To depend on a particular unreleased version, you can do the following:

```starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")


# Update the SHA and VERSION to the lastest version available here:
# https://github.com/bazelbuild/rules_python/releases.

SHA="84aec9e21cc56fbc7f1335035a71c850d1b9b5cc6ff497306f84cced9a769841"

VERSION="0.23.1"

http_archive(
    name = "rules_python",
    sha256 = SHA,
    strip_prefix = "rules_python-{}".format(VERSION),
    url = "https://github.com/bazelbuild/rules_python/releases/download/{}/rules_python-{}.tar.gz".format(VERSION,VERSION),
)

load("@rules_python//python:repositories.bzl", "py_repositories")

py_repositories()
```

### Workspace toolchain registration

To register a hermetic Python toolchain rather than rely on a system-installed interpreter for runtime execution, you can add to the `WORKSPACE` file:

```starlark
load("@rules_python//python:repositories.bzl", "python_register_toolchains")

python_register_toolchains(
    name = "python_3_11",
    # Available versions are listed in @rules_python//python:versions.bzl.
    # We recommend using the same version your team is already standardized on.
    python_version = "3.11",
)

load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    ...
    python_interpreter_target = "@python_3_11_host//:python",
    ...
)
```

After registration, your Python targets will use the toolchain's interpreter during execution, but a system-installed interpreter
is still used to 'bootstrap' Python targets (see https://github.com/bazelbuild/rules_python/issues/691).
You may also find some quirks while using this toolchain. Please refer to [python-build-standalone documentation's _Quirks_ section](https://gregoryszorc.com/docs/python-build-standalone/main/quirks.html).

## Autodetecting toolchain

The autodetecting toolchain is a deprecated toolchain that is built into Bazel.
It's name is a bit misleading: it doesn't autodetect anything. All it does is
use `python3` from the environment a binary runs within. This provides extremely
limited functionality to the rules (at build time, nothing is knowable about
the Python runtime).

Bazel itself automatically registers `@bazel_tools//tools/python:autodetecting_toolchain`
as the lowest priority toolchain. For WORKSPACE builds, if no other toolchain
is registered, that toolchain will be used. For bzlmod builds, rules_python
automatically registers a higher-priority toolchain; it won't be used unless
there is a toolchain misconfiguration somewhere.

To aid migration off the Bazel-builtin toolchain, rules_python provides
{bzl:obj}`@rules_python//python/runtime_env_toolchains:all`. This is an equivalent
toolchain, but is implemented using rules_python's objects.


## Custom toolchains

While rules_python provides toolchains by default, it is not required to use
them, and you can define your own toolchains to use instead. This section
gives an introduction for how to define them yourself.

:::{note}
* Defining your own toolchains is an advanced feature.
* APIs used for defining them are less stable and may change more often.
:::

Under the hood, there are multiple toolchains that comprise the different
information necessary to build Python targets. Each one has an
associated _toolchain type_ that identifies it. We call the collection of these
toolchains a "toolchain suite".

One of the underlying design goals of the toolchains is to support complex and
bespoke environments. Such environments may use an arbitrary combination of
{obj}`RBE`, cross-platform building, multiple Python versions,
building Python from source, embeding Python (as opposed to building separate
interpreters), using prebuilt binaries, or using binaries built from source. To
that end, many of the attributes they accept, and fields they provide, are
optional.

### Target toolchain type

The target toolchain type is {obj}`//python:toolchain_type`, and it
is for _target configuration_ runtime information, e.g., the Python version
and interpreter binary that a program will use.

The is typically implemented using {obj}`py_runtime()`, which
provides the {obj}`PyRuntimeInfo` provider. For historical reasons from the
Python 2 transition, `py_runtime` is wrapped in {obj}`py_runtime_pair`,
which provides {obj}`ToolchainInfo` with the field `py3_runtime`, which is an
instance of `PyRuntimeInfo`.

This toolchain type is intended to hold only _target configuration_ values. As
such, when defining its associated {external:bzl:obj}`toolchain` target, only
set {external:bzl:obj}`toolchain.target_compatible_with` and/or
{external:bzl:obj}`toolchain.target_settings` constraints; there is no need to
set {external:bzl:obj}`toolchain.exec_compatible_with`.

### Python C toolchain type

The Python C toolchain type ("py cc") is {obj}`//python/cc:toolchain_type`, and
it has C/C++ information for the _target configuration_, e.g. the C headers that
provide `Python.h`.

This is typically implemented using {obj}`py_cc_toolchain()`, which provides
{obj}`ToolchainInfo` with the field `py_cc_toolchain` set, which is a
{obj}`PyCcToolchainInfo` provider instance. 

This toolchain type is intended to hold only _target configuration_ values
relating to the C/C++ information for the Python runtime. As such, when defining
its associated {external:obj}`toolchain` target, only set
{external:bzl:obj}`toolchain.target_compatible_with` and/or
{external:bzl:obj}`toolchain.target_settings` constraints; there is no need to
set {external:bzl:obj}`toolchain.exec_compatible_with`.

### Exec tools toolchain type

The exec tools toolchain type is {obj}`//python:exec_tools_toolchain_type`,
and it is for supporting tools for _building_ programs, e.g. the binary to
precompile code at build time.

This toolchain type is intended to hold only _exec configuration_ values --
usually tools (prebuilt or from-source) used to build Python targets.

This is typically implemented using {obj}`py_exec_tools_toolchain`, which
provides {obj}`ToolchainInfo` with the field `exec_tools` set, which is an
instance of {obj}`PyExecToolsInfo`.

The toolchain constraints of this toolchain type can be a bit more nuanced than
the other toolchain types. Typically, you set
{external:bzl:obj}`toolchain.target_settings` to the Python version the tools
are for, and {external:bzl:obj}`toolchain.exec_compatible_with` to the platform
they can run on. This allows the toolchain to first be considered based on the
target configuration (e.g. Python version), then for one to be chosen based on
finding one compatible with the available host platforms to run the tool on.

However, what `target_compatible_with`/`target_settings` and
`exec_compatible_with` values to use depend on details of the tools being used.
For example:
* If you had a precompiler that supported any version of Python, then
  putting the Python version in `target_settings` is unnecessary.
* If you had a prebuilt polyglot precompiler binary that could run on any
  platform, then setting `exec_compatible_with` is unnecessary.

This can work because, when the rules invoke these build tools, they pass along
all necessary information so that the tool can be entirely independent of the
target configuration being built for.

Alternatively, if you had a precompiler that only ran on linux, and only
produced valid output for programs intended to run on linux, then _both_
`exec_compatible_with` and `target_compatible_with` must be set to linux.

### Custom toolchain example

Here, we show an example for a semi-complicated toolchain suite, one that is:

* A CPython-based interpreter
* For Python version 3.12.0
* Using an in-build interpreter built from source
* That only runs on Linux
* Using a prebuilt precompiler that only runs on Linux, and only produces byte
  code valid for 3.12
* With the exec tools interpreter disabled (unnecessary with a prebuild
  precompiler)
* Providing C headers and libraries

Defining toolchains for this might look something like this:

```
# -------------------------------------------------------
# File: toolchain_impl/BUILD
# Contains the tool definitions (runtime, headers, libs).
# -------------------------------------------------------
load("@rules_python//python:py_cc_toolchain.bzl", "py_cc_toolchain")
load("@rules_python//python:py_exec_tools_toolchain.bzl", "py_exec_tools_toolchain")
load("@rules_python//python:py_runtime.bzl", "py_runtime")
load("@rules_python//python:py_runtime_pair.bzl", "py_runtime_pair")

MAJOR = 3
MINOR = 12
MICRO = 0

py_runtime(
    name = "runtime",
    interpreter = ":python",
    interpreter_version_info = {
        "major": str(MAJOR),
        "minor": str(MINOR),
        "micro": str(MICRO),
    }
    implementation = "cpython"
)
py_runtime_pair(
    name = "runtime_pair",
    py3_runtime = ":runtime"
)

py_cc_toolchain(
    name = "py_cc_toolchain_impl",
    headers = ":headers",
    libs = ":libs",
    python_version = "{}.{}".format(MAJOR, MINOR)
)

py_exec_tools_toolchain(
    name = "exec_tools_toolchain_impl",
    exec_interpreter = "@rules_python/python:none",
    precompiler = "precompiler-cpython-3.12"
)

cc_binary(name = "python3.12", ...)
cc_library(name = "headers", ...)
cc_library(name = "libs", ...)

# ------------------------------------------------------------------
# File: toolchains/BUILD
# Putting toolchain() calls in a separate package from the toolchain
# implementations minimizes Bazel loading overhead.
# ------------------------------------------------------------------

toolchain(
    name = "runtime_toolchain",
    toolchain = "//toolchain_impl:runtime_pair",
    toolchain_type = "@rules_python//python:toolchain_type",
    target_compatible_with = ["@platforms/os:linux"]
)
toolchain(
    name = "py_cc_toolchain",
    toolchain = "//toolchain_impl:py_cc_toolchain_impl",
    toolchain_type = "@rules_python//python/cc:toolchain_type",
    target_compatible_with = ["@platforms/os:linux"]
)

toolchain(
    name = "exec_tools_toolchain",
    toolchain = "//toolchain_impl:exec_tools_toolchain_impl",
    toolchain_type = "@rules_python//python:exec_tools_toolchain_type",
    target_settings = [
        "@rules_python//python/config_settings:is_python_3.12",
    ],
    exec_comaptible_with = ["@platforms/os:linux"]
)

# -----------------------------------------------
# File: MODULE.bazel or WORKSPACE.bazel
# These toolchains will considered before others.
# -----------------------------------------------
register_toolchains("//toolchains:all")
```

When registering custom toolchains, be aware of the the [toolchain registration
order](https://bazel.build/extending/toolchains#toolchain-resolution). In brief,
toolchain order is the BFS-order of the modules; see the bazel docs for a more
detailed description.

:::{note}
The toolchain() calls should be in a separate BUILD file from everything else.
This avoids Bazel having to perform unnecessary work when it discovers the list
of available toolchains.
:::

## Toolchain selection flags

Currently the following flags are used to influence toolchain selection:
* {obj}`--@rules_python//python/config_settings:py_linux_libc` for selecting the Linux libc variant.
* {obj}`--@rules_python//python/config_settings:py_freethreaded` for selecting
  the freethreaded experimental Python builds available from `3.13.0` onwards.
# Installation and Usage

## Example

Examples of using Gazelle with Python can be found in the `rules_python`
repo:

* bzlmod: {gh-path}`gazelle/examples/bzlmod_build_file_generation`
* WORKSPACE: {gh-path}`examples/build_file_generation`

:::{note}
The following documentation covers using bzlmod.
:::


## Adding Gazelle to your project

First, you'll need to add Gazelle to your `MODULE.bazel` file. Get the current
version of [Gazelle][bcr-gazelle] from the [Bazel Central Registry][bcr]. Then
do the same for [`rules_python`][bcr-rules-python] and
[`rules_python_gazelle_plugin`][bcr-rules-python-gazelle-plugin].

[bcr-gazelle]: https://registry.bazel.build/modules/gazelle
[bcr]: https://registry.bazel.build/
[bcr-rules-python]: https://registry.bazel.build/modules/rules_python
[bcr-rules-python-gazelle-plugin]: https://registry.bazel.build/modules/rules_python_gazelle_plugin

Here is a snippet of a `MODULE.bazel` file. Note that most of it is just
general config for `rules_python` itself - the Gazelle plugin is only two lines
at the end.

```starlark
################################################
## START rules_python CONFIG                  ##
## See the main rules_python docs for details ##
################################################
bazel_dep(name = "rules_python", version = "1.5.1")

python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(python_version = "3.12.2")
use_repo(python, "python_3_12_2")

pip = use_extension("@rules_python//python:extensions.bzl", "pip")
pip.parse(
    hub_name = "pip",
    requirements_lock = "//:requirements_lock.txt",
    requirements_windows = "//:requirements_windows.txt",
)
use_repo(pip, "pip")

##############################################
## START rules_python_gazelle_plugin CONFIG ##
##############################################

# The Gazelle plugin depends on Gazelle.
bazel_dep(name = "gazelle", version = "0.33.0", repo_name = "bazel_gazelle")

# Typically rules_python_gazelle_plugin is version matched to rules_python.
bazel_dep(name = "rules_python_gazelle_plugin", version = "1.5.1")
```

Next, we'll fetch metadata about your Python dependencies, so that gazelle can
determine which package a given import statement comes from. This is provided
by the `modules_mapping` rule. We'll make a target for consuming this
`modules_mapping`, and writing it as a manifest file for Gazelle to read.
This is checked into the repo for speed, as it takes some time to calculate
in a large monorepo.

Gazelle will walk up the filesystem from a Python file to find this metadata,
looking for a file called `gazelle_python.yaml` in an ancestor folder
of the Python code. Create an empty file with this name. It might be next
to your `requirements.txt` file. (You can just use {command}`touch` at
this point, it just needs to exist.)

To keep the metadata updated, put this in your `BUILD.bazel` file next
to `gazelle_python.yaml`:

```starlark
# `@pip` is the hub_name from pip.parse in MODULE.bazel.
load("@pip//:requirements.bzl", "all_whl_requirements")
load("@rules_python_gazelle_plugin//manifest:defs.bzl", "gazelle_python_manifest")
load("@rules_python_gazelle_plugin//modules_mapping:def.bzl", "modules_mapping")

# This rule fetches the metadata for python packages we depend on. That data is
# required for the gazelle_python_manifest rule to update our manifest file.
modules_mapping(
    name = "modules_map",
    wheels = all_whl_requirements,

    # include_stub_packages: bool (default: False)
    # If set to True, this flag automatically includes any corresponding type stub packages
    # for the third-party libraries that are present and used. For example, if you have
    # `boto3` as a dependency, and this flag is enabled, the corresponding `boto3-stubs`
    # package will be automatically included in the BUILD file.
    # Enabling this feature helps ensure that type hints and stubs are readily available
    # for tools like type checkers and IDEs, improving the development experience and
    # reducing manual overhead in managing separate stub packages.
    include_stub_packages = True,
)

# Gazelle python extension needs a manifest file mapping from
# an import to the installed package that provides it.
# This macro produces two targets:
# - //:gazelle_python_manifest.update can be used with `bazel run`
#   to recalculate the manifest
# - //:gazelle_python_manifest.test is a test target ensuring that
#   the manifest doesn't need to be updated
gazelle_python_manifest(
    name = "gazelle_python_manifest",
    modules_mapping = ":modules_map",

    # This is what we called our `pip.parse` rule in MODULE.bazel, where third-party
    # python libraries are loaded in BUILD files.
    pip_repository_name = "pip",

    # This should point to wherever we declare our python dependencies
    # (the same as what we passed to the modules_mapping rule in WORKSPACE)
    # This argument is optional. If provided, the `.test` target is very
    # fast because it just has to check an integrity field. If not provided,
    # the integrity field is not added to the manifest which can help avoid
    # merge conflicts in large repos.
    requirements = "//:requirements_lock.txt",
)
```

Finally, you create a target that you'll invoke to run the Gazelle tool
with the `rules_python` extension included. This typically goes in your root
`/BUILD.bazel` file:

```starlark
load("@bazel_gazelle//:def.bzl", "gazelle", "gazelle_binary")

gazelle_binary(
    name = "gazelle_multilang",
    languages = [
        # List of language plugins.
        # If you want to generate py_proto_library targets (PR #3057), then
        # the proto language plugin _must_ come before the rules_python plugin.
        #"@bazel_gazelle//language/proto",
        "@rules_python_gazelle_plugin//python",
    ],
)

gazelle(
    name = "gazelle",
    gazelle = ":gazelle_multilang",
)
```

That's it, now you can finally run `bazel run //:gazelle` anytime
you edit Python code, and it should update your `BUILD` files correctly.


## Target Types and How They're Generated

### Libraries

Python source files are those ending in `.py` that are not matched as a test
file via the {term}`# gazelle:python_test_file_pattern value` directive. By default,
python source files are all `*.py` files except for `*_test.py` and
`test_*.py`.

First, we look for the nearest ancestor `BUILD(.bazel)` file starting from
the folder containing the Python source file.

+ In `package` generation mode, if there is no {bzl:obj}`py_library` in this
  `BUILD(.bazel)` file, one is created using the package name as the target's
  name. This makes it the default target in the package. Next, all source
  files are collected into the `srcs` of the {bzl:obj}`py_library`.
+ In `project` generation mode, all source files in subdirectories (that don't
  have `BUILD(.bazel)` files) are also collected.
+ In `file` generation mode, each python source file is given its own target.

Finally, the `import` statements in the source files are parsed and
dependencies are added to the `deps` attribute of the target.


### Tests

A {bzl:obj}`py_test` target is added to the `BUILD(.bazel)` file when gazelle
encounters a file named `__test__.py` or when files matching the
{term}`# gazelle:python_test_file_pattern value` directive are found.

For example, if we had a folder that is a package named "foo" we could have a
Python file named `foo_test.py` and gazelle would create a {bzl:obj}`py_test`
block for the file.

The following is an example of a {bzl:obj}`py_test` target that gazelle would
add when it encounters a file named `__test__.py`.

```starlark
py_test(
    name = "build_file_generation_test",
    srcs = ["__test__.py"],
    main = "__test__.py",
    deps = [":build_file_generation"],
)
```

You can control the naming convention for test targets using the
{term}`# gazelle:python_test_naming_convention value` directive.


### Binaries

When a `__main__.py` file is encountered, this indicates the entry point
of a Python program. A {bzl:obj}`py_binary` target will be created, named
`[package]_bin`.

When no such entry point exists, Gazelle will look for a line like this in
the top level in every module:

```python
if __name == "__main__":
```

Gazelle will create a {bzl:obj}`py_binary` target for every module with such
a line, with the target name the same as the module name.

If the {term}`# gazelle:python_generation_mode value` directive is set to `file`, then
instead of one {bzl:obj}`py_binary` target per module, Gazelle will create
one {bzl:obj}`py_binary` target for each file with such a line, and the name
of the target will match the name of the script.

:::{note}
It's possible for another script to depend on a {bzl:obj}`py_binary` target
and import from the {bzl:obj}`py_binary`'s scripts. This can have possible
negative effects on Bazel analysis time and runfiles size compared to
depending on a {bzl:obj}`py_library` target. The simplest way to avoid these
negative effects is to extract library code into a separate script without a
`main` line. Gazelle will then create a {bzl:obj}`py_library` target for
that library code, and other scripts can depend on that {bzl:obj}`py_library`
target.
:::

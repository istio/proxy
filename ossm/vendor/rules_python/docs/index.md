# Python Rules for Bazel

`rules_python` is the home for four major components with varying maturity levels.

:::{topic} Core rules

The core Python rules -- `py_library`, `py_binary`, `py_test`,
and related symbols that provide the basis for Python
support in Bazel.

When using Bazel 6 (or earlier), the core rules are bundled into the Bazel binary, and the symbols
in this repository are simple aliases. On Bazel 7 and above, `rules_python` uses
a separate Starlark implementation;
see {ref}`Migrating from the Bundled Rules` below.

This repository follows
[semantic versioning](https://semver.org) and the breaking change policy
outlined in the [support](support) page.

:::

:::{topic} PyPI integration

Package installation rules for integrating with PyPI and other Simple API-
compatible indexes.

These rules work and can be used in production, but the cross-platform building
that supports pulling PyPI dependencies for a target platform that is different
from the host platform is still in beta, and the APIs that are subject to potential
change are marked as `experimental`.

:::

:::{topic} Sphinxdocs

`sphinxdocs` rules allow users to generate documentation using Sphinx powered by Bazel, with additional functionality for documenting
Starlark and Bazel code.

The functionality is exposed because other projects find it useful, but
it is available "as is", and **the semantic versioning and
compatibility policy used by `rules_python` does not apply**.

:::

:::{topic} Gazelle plugin

`gazelle` plugin for generating `BUILD.bazel` files based on Python source
code.

This is available "as is", and the semantic versioning used by `rules_python` does
not apply.

:::

The Bazel community maintains this repository. Neither Google nor the Bazel
team provides support for the code. However, this repository is part of the
test suite used to vet new Bazel releases. See {gh-path}`How to contribute
<CONTRIBUTING.md>` for information on our development workflow.

## Examples

This documentation is an example of `sphinxdocs` rules and the rest of the
components have examples in the {gh-path}`examples` directory.

## Migrating from the bundled rules

The core rules are currently available in Bazel as built-in symbols, but this
form is deprecated. Instead, you should depend on rules_python in your
`WORKSPACE` or `MODULE.bazel` file and load the Python rules from
`@rules_python//python:<name>.bzl` or load paths described in the API documentation.

A [buildifier](https://github.com/bazelbuild/buildtools/blob/master/buildifier/README.md)
fix is available to automatically migrate `BUILD` and `.bzl` files to add the
appropriate `load()` statements and rewrite uses of `native.py_*`.

```sh
# Also consider using the -r flag to modify an entire workspace.
buildifier --lint=fix --warnings=native-py <files>
```

Currently, the `WORKSPACE` file needs to be updated manually as per
[Getting started](getting-started).

Note that Starlark-defined bundled symbols underneath
`@bazel_tools//tools/python` are also deprecated. These are not yet rewritten
by buildifier.

## Migrating to bzlmod

See {gh-path}`Bzlmod support <BZLMOD_SUPPORT.md>` for any behavioral differences between
`bzlmod` and `WORKSPACE`.


```{toctree}
:hidden:
self
getting-started
pypi/index
Toolchains <toolchains>
coverage
precompiling
gazelle/docs/index
REPL <repl>
Extending <extending>
How-to Guides <howto/index>
Contributing <contributing>
devguide
support
Changelog <changelog>
api/index
environment-variables
Sphinxdocs <sphinxdocs/index>
glossary
genindex
```

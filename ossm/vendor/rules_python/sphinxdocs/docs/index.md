# Docgen using Sphinx with Bazel

The `sphinxdocs` project allows using Bazel to run Sphinx to generate
documentation. It comes with:

* Rules for running Sphinx
* Rules for generating documentation for Starlark code.
* A Sphinx plugin for documenting Starlark and Bazel objects.
* Rules for readthedocs build integration.

While it is primarily oriented towards docgen for Starlark code, the core of it
is agnostic as to what is being documented.


```{toctree}
:hidden:

starlark-docgen
sphinx-bzl
readthedocs
```

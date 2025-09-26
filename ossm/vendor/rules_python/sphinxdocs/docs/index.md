# Docgen using Sphinx with Bazel

The `sphinxdocs` project allows using Bazel to run Sphinx to generate
documentation. It comes with:

* Rules for running Sphinx
* Rules for generating documentation for Starlark code.
* A Sphinx plugin for documenting Starlark and Bazel objects.
* Rules for readthedocs build integration.

While it is primarily oriented towards docgen for Starlark code, the core of it
is agnostic as to what is being documented.

### Optimization

Normally, Sphinx keeps various cache files to improve incremental building.
Unfortunately, programs performing their own caching don't interact well
with Bazel's model of precisely declaring and strictly enforcing what are
inputs, what are outputs, and what files are available when running a program.
The net effect is programs don't have a prior invocation's cache files
available.

There are two mechanisms available to make some cache available to Sphinx under
Bazel:

* Disable sandboxing, which allows some files from prior invocations to be
  visible to subsequent invocations. This can be done multiple ways:
  * Set `tags = ["no-sandbox"]` on the `sphinx_docs` target
  * `--modify_execution_info=SphinxBuildDocs=+no-sandbox` (Bazel flag)
  * `--strategy=SphinxBuildDocs=local` (Bazel flag)
* Use persistent workers (enabled by default) by setting
  `allow_persistent_workers=True` on the `sphinx_docs` target. Note that other
  Bazel flags can disable using workers even if an action supports it. Setting
  `--strategy=SphinxBuildDocs=dynamic,worker,local,sandbox` should tell Bazel
  to use workers if possible, otherwise fallback to non-worker invocations.


```{toctree}
:hidden:

starlark-docgen
sphinx-bzl
readthedocs
```

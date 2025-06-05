:::{bzl:currentfile} //sphinxdocs:BUILD.bazel
:::

# //sphinxdocs

:::{bzl:flag} extra_defines
Additional `-D` values to add to every Sphinx build.

This is a list flag. Multiple uses are accumulated.

This is most useful for overriding e.g. the version when performing
release builds.
:::

:::{bzl:flag} extra_env
Additional environment variables to for every Sphinx build.

This is a list flag. Multiple uses are accumulated. Values are `key=value`
format.
:::

:::{bzl:flag} quiet
Whether to add the `-q` arg to Sphinx invocations.

This is a boolean flag.

This is useful for debugging invocations or developing extensions. The Sphinx
`-q` flag causes sphinx to produce additional output on stdout.
:::

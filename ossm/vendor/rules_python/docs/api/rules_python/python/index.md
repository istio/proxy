:::{default-domain} bzl
:::
:::{bzl:currentfile} //python:BUILD.bazel
:::

# //python

:::{bzl:target} toolchain_type

Identifier for the toolchain type for the target platform.

This toolchain type gives information about the runtime for the target platform.
It is typically implemented by the {obj}`py_runtime` rule.

::::{seealso}
{any}`Custom Toolchains` for how to define custom toolchains
::::

:::

:::{bzl:target} exec_tools_toolchain_type

Identifier for the toolchain type for exec tools used to build Python targets.

This toolchain type gives information about tools needed to build Python targets
at build time. It is typically implemented by the {obj}`py_exec_tools_toolchain`
rule.

::::{seealso}
{any}`Custom Toolchains` for how to define custom toolchains
::::
:::

:::{bzl:target} current_py_toolchain

Helper target to resolve to the consumer's current Python toolchain. This target
provides:

* {obj}`PyRuntimeInfo`: The consuming target's target toolchain information

:::

::::{target} autodetecting_toolchain

Legacy toolchain; despite its name, it doesn't autodetect anything.

:::{deprecated} 0.34.0

Use {obj}`@rules_python//python/runtime_env_toolchains:all` instead.
:::
::::

:::{target} none
A special target so that label attributes with default values can be set to
`None`.

Bazel interprets `None` to mean "use the default value", which
makes it impossible to have a label attribute with a default value that is
optional. To work around this, a target with a special provider is used;
internally rules check for this, then treat the value as `None`.

::::{versionadded} 0.36.0
::::

:::

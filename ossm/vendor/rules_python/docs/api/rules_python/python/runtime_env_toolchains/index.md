:::{default-domain} bzl
:::
:::{bzl:currentfile} //python/runtime_env_toolchains:BUILD.bazel
:::

# //python/runtime_env_toolchains

::::{target} all

A set of toolchains that invoke `python3` from the runtime environment (i.e
after building).

:::{note}
These toolchains do not provide any build-time information, including but not
limited to the Python version or C headers. As such, they cannot be used
for e.g. precompiling, building Python C extension modules, or anything else
that requires information about the Python runtime at build time. Under the
hood, these simply create a fake "interpreter" that calls `python3` that
built programs use to run themselves.
:::

This is only provided to aid migration off the builtin Bazel toolchain 
(`@bazel_tools//python:autodetecting_toolchain`), and is largely only applicable
to WORKSPACE builds.

To use this target, register it as a toolchain in WORKSPACE or MODULE.bazel:

:::
register_toolchains("@rules_python//python/runtime_env_toolchains:all")
:::

The benefit of this target over the legacy targets is this defines additional
toolchain types that rules_python needs. This prevents toolchain resolution from
continuing to search elsewhere (e.g. potentially incurring a download of the
hermetic runtimes when they won't be used).

:::{deprecated} 0.34.0

Switch to using a hermetic toolchain or manual toolchain configuration instead.
:::

:::{versionadded} 0.34.0
:::
::::

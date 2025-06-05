# Writing a custom C++ toolchain

This example shows how to define and use a simple custom C++ toolchain.

Output is non-functional: simple scripts replace compilation and linking
with `I compiled!` and `I linked!` messages.

[BUILD](BUILD) provides detailed implementation walkthrough. The fundamental
sequence is:

1. Define the toolchain
1. Define how to invoke the toolchain.

`1` is C++-specific: the logic and structure depends specifically on C++'s
language model. Other languages have their own models.

`2` supports two variations. `--crosstool_top` / `--cpu`, the legacy version,
is C++-specific. `--platforms`, the modern version, is much more generic and
supports all languages and features like [incompatible target
skipping](https://docs.bazel.build/versions/master/platforms.html#skipping-incompatible-targets). See
[Building with
Platforms](https://docs.bazel.build/versions/master/platforms-intro.html) and
its [C++
notes](https://docs.bazel.build/versions/master/platforms-intro.html#c) for
full review.

## Building with the default toolchain

```
$ bazel clean
$ bazel build //examples/custom_toolchain:buildme
$ file bazel-bin/examples/custom_toolchain/libbuildme.a
bazel-bin/examples/custom_toolchain/libbuildme.a: current ar archive
```

## Custom toolchain with platforms

This mode requires `--incompatible_enable_cc_toolchain_resolution`. Without this
flag, `--platforms` and `--extra_toolchains` are ignored and the default
toolchain triggers.

```
$ bazel clean
$ bazel build //examples/custom_toolchain:buildme --platforms=//examples/custom_toolchain:x86_platform --extra_toolchains=//examples/custom_toolchain:platform_based_toolchain --incompatible_enable_cc_toolchain_resolution
DEBUG: /usr/local/google/home/gregce/bazel/rules_cc/examples/custom_toolchain/toolchain_config.bzl:17:10: Invoking my custom toolchain!
INFO: From Compiling examples/custom_toolchain/buildme.cc:
examples/custom_toolchain/sample_compiler: running sample cc_library compiler (produces .o output).
INFO: From Linking examples/custom_toolchain/libbuildme.a:
examples/custom_toolchain/sample_linker: running sample cc_library linker (produces .a output).

$ cat bazel-bin/examples/custom_toolchain/libbuildme.a
examples/custom_toolchain/sample_linker: sample output
```

This example uses a long command line for demonstration purposes. A real project
would [register toolchains](https://docs.bazel.build/versions/master/toolchains.html#registering-and-building-with-toolchains)
in `WORKSPACE` and auto-set
`--incompatible_enable_cc_toolchain_resolution`. That reduces the command to:

```
$ bazel build //examples/custom_toolchain:buildme --platforms=//examples/custom_toolchain:x86_platform
```

## Custom toolchain with legacy selection:

```
$ bazel clean
$ bazel build //examples/custom_toolchain:buildme --crosstool_top=//examples/custom_toolchain:legacy_selector --cpu=x86
DEBUG: /usr/local/google/home/gregce/bazel/rules_cc/examples/custom_toolchain/toolchain_config.bzl:17:10: Invoking my custom toolchain!
INFO: From Compiling examples/custom_toolchain/buildme.cc:
examples/custom_toolchain/sample_compiler: running sample cc_library compiler (produces .o output).
INFO: From Linking examples/custom_toolchain/libbuildme.a:
examples/custom_toolchain/sample_linker: running sample cc_library linker (produces .a output).

$ cat bazel-bin/examples/custom_toolchain/libbuildme.a
examples/custom_toolchain/sample_linker: sample output
```


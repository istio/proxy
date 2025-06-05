# Apple Support for [Bazel](https://bazel.build)

This repository contains the [Apple CC toolchain](#toolchain-setup),
Apple related [platforms](platforms/BUILD) and
[constraints](constraints/BUILD) definitions, and small helper functions
for rules authors targeting Apple platforms.

If you want to build iOS, tvOS, visionOS, watchOS, or macOS apps, use
[`rules_apple`][rules_apple].

If you want to build Swift use
[`rules_swift`](https://github.com/bazelbuild/rules_swift).

See [the documentation](doc) for the helper rules provided by this
repository.

## Installation

Copy the `MODULE.bazel` or `WORKSPACE` snippets from [the releases
page](https://github.com/bazelbuild/apple_support/releases) into your
project.

## Toolchain setup

The Apple CC toolchain in this repository provides toolchains for
building for Apple platforms besides macOS. Since Bazel 7 this toolchain
is required when targeting those platforms but the toolchain also
supports Bazel 6.

NOTE: This toolchain requires a full Xcode installation, not just the
Xcode Command Line Tools. If you only need to build for macOS and don't
want to require a full Xcode installation in your build, use the builtin
Unix toolchain provided by Bazel.

### Bazel 7+ Setup

For Bazel 7+ the only setup that is required is to have `apple_support`
in your `MODULE.bazel` (even if you're not referencing it directly) or
`WORKSPACE`, which you can copy from [the releases
page](https://github.com/bazelbuild/apple_support/releases) into your
project.

If you also depend on `rules_cc`, `apple_support` must come _above_
`rules_cc` in your `MODULE.bazel` or `WORKSPACE` file because Bazel
selects toolchains based on which is registered first.

### Bazel 6 Setup

For Bazel 6, pull this repository into your build and add this to your
`.bazelrc`:

```bzl
build --enable_platform_specific_config
build:macos --apple_crosstool_top=@local_config_apple_cc//:toolchain
build:macos --crosstool_top=@local_config_apple_cc//:toolchain
build:macos --host_crosstool_top=@local_config_apple_cc//:toolchain
```

This ensures that all rules provided by [`rules_apple`][rules_apple], as
well as other rules like `cc_binary`, all use the toolchain provided by
this repository when building on macOS.

If you're using bzlmod with Bazel 6 and the `--crosstool_top`
configurations you must expose the `local_config_apple_cc` repository to
your project by putting this in your `MODULE.bazel`:

```bzl
apple_cc_configure = use_extension("@build_bazel_apple_support//crosstool:setup.bzl", "apple_cc_configure_extension")
use_repo(apple_cc_configure, "local_config_apple_cc")
```

## Toolchain configuration

There are many different flags you can flip to configure how the
toolchain works. Here are some of the more commonly useful ones:

- Setting `DEVELOPER_DIR` in the environment. This is recommended so
  that the toolchain can be invalidated when the `DEVELOPER_DIR`
  changes, which ensures that toolchain binaries will be rebuilt with
  the new version of Xcode so that caches are correctly shared across
  machines.
- Setting `BAZEL_ALLOW_NON_APPLICATIONS_XCODE=1` in the environment (or
  using `--repo_env`) allows the toolchain to discover Xcode versions
  outside of the `/Applications` directory to avoid header inclusion
  errors from bazel. This is not enabled by default because
  `/Applications` is the standard directory, and this improves toolchain
  setup performance.

[rules_apple]: https://github.com/bazelbuild/rules_apple

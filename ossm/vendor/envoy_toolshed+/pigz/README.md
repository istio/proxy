# Hermetic pigz (`//pigz`)

[pigz](https://zlib.net/pigz/), a parallel gzip, exposed as `//pigz:toolchain_type` and, resolved
for the platform it runs on, as the executable target `//pigz`.

## Runtime layout

Each `bins-v*` release publishes `pigz-<version>-linux-{x86_64,aarch64}.tar.zst`:

- `BUILD.bazel`
- `bin/pigz`: the `pigz` module, with zlib-ng (zlib-compatible, for faster compression at the
  same level and format) in place of zlib, and zopfli, linked statically; built against
  toolshed's glibc 2.31 sysroot and stripped.
- `share/doc/`: pigz, zlib-ng and zopfli licenses.

## Consuming pigz

toolshed registers the toolchains, so a dependency on `envoy_toolshed` is enough. Use `//pigz` as
a tool, eg to compress a `pkg_tar` with all the cores:

```starlark
pkg_tar(
    name = "layer",
    srcs = [...],
    compressor = "@envoy_toolshed//pigz",
    # No name or time in the gzip header, for reproducible output.
    compressor_args = "-n",
    extension = "tar.gz",
)
```

As a tool (`cfg = "exec"`), `//pigz` is resolved for the platform the action runs on: the
toolchains match on `target_compatible_with`, which in the exec configuration is that platform.

`//pigz` resolves, in order:

1. the prebuilt for the platform (Linux x86_64 and aarch64, once a bins release publishes them);
2. a host pigz, if the root module sets one (`host_path`, below);
3. a source toolchain, if the consumer registers one;
4. otherwise, it fails, naming the reason: toolshed does not build pigz for consumers, so that
   they never fetch and compile it unless they ask to.

To use a pigz already on the host where there is no prebuilt (eg macOS), rather than building it,
set its absolute path:

```starlark
pigz_prebuilt = use_extension("@envoy_toolshed//pigz:extensions.bzl", "pigz_prebuilt_extension")
pigz_prebuilt.setup(host_path = "/opt/homebrew/bin/pigz")
```

This adds a toolchain for the host's OS and CPU, unless there is an enabled prebuilt for it, which
takes precedence (to use the host pigz on Linux, disable the prebuilt for it, eg
`linux_x86_64_sha256 = ""`). The path must exist (it
is watched, so a change re-evaluates the toolchains), and the binary runs wherever the action
does, so with remote execution it must run there too.

To build pigz from source where there is no prebuilt (eg macOS), depend on the `pigz` module and
register a source toolchain after the prebuilts. A root module's toolchains are registered before
toolshed's, so register the prebuilts first, and constrain the source toolchain to where it should
build:

```starlark
# MODULE.bazel
bazel_dep(name = "pigz", version = "2.8.bcr.3")

pigz_prebuilt = use_extension("@envoy_toolshed//pigz:extensions.bzl", "pigz_prebuilt_extension")
use_repo(pigz_prebuilt, "pigz_toolchains")

register_toolchains(
    "@pigz_toolchains//:all",
    "//tools/pigz:source_toolchain",
)
```

```starlark
# tools/pigz/BUILD
load("@envoy_toolshed//pigz:defs.bzl", "PIGZ_TOOLCHAIN_TYPE", "pigz_toolchain")

pigz_toolchain(
    name = "source_impl",
    pigz = "@pigz",
)

toolchain(
    name = "source_toolchain",
    target_compatible_with = ["@platforms//os:macos"],
    toolchain = ":source_impl",
    toolchain_type = PIGZ_TOOLCHAIN_TYPE,
)
```

## Building

toolshed itself builds the `pigz` module (a dev dependency): for `//pigz/source:pigz_packages`,
the published tarballs, and as the source toolchain of its own tests (also on macOS). The release
preparation sets `VERSIONS["pigz_sha256"]` from a bins release, enabling the prebuilt toolchains.
Keep `VERSIONS["pigz"]` in sync with the `pigz` bazel_dep.

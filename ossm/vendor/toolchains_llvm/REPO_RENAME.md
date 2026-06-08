# Rename from bazel-toolchain to toolchains_llvm

As part of the transfer to the bazel-contrib org, the repo has been renamed to
`toolchains_llvm`. This has affected the dynamically generated source archives
which now have a different tree prefix, and consequently, a different shasum.

From release 0.10.1 onwards, the releases have all generated a release artifact
which is guaranteed to be stable. But for prior releases, users need to change
the `shasum` and `strip_prefix` attributes for referencing this repo.

0.10.0:

```bzl
  strip_prefix = "toolchains_llvm-0.10.0",
  shasum = "a2877b8bf596ee4c0310b50463796efd8f360dcb087675e9101e15c39e03d7ea",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.10.0.tar.gz",
```

0.9:

```bzl
  strip_prefix = "toolchains_llvm-0.9",
  shasum = "b2d168315dd0785f170b2b306b86e577c36e812b8f8b05568f9403141f2c24dd",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.9.tar.gz",
```

0.8.2:

```bzl
  strip_prefix = "toolchains_llvm-0.8.2",
  shasum = "3e251524b3e8f3b9ec93848e5267c168424f43b7b554efc983a5291c33d78cde",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.8.2.tar.gz",
```

0.8.1:

```bzl
  strip_prefix = "toolchains_llvm-0.8.1",
  shasum = "3bb45f480e3eb198f39fc97e91df2c2fc0beaabbea620ba3034ac505786a1813",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.8.1.tar.gz",
```

0.8:

```bzl
  strip_prefix = "toolchains_llvm-0.8",
  shasum = "f121449dd565d59274b7421a62f3ed1f16ad7ceab4575c5b34f882ba441093bd",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.8.tar.gz",
```

0.7.2:

```bzl
  strip_prefix = "toolchains_llvm-0.7.2",
  shasum = "ea7d247dd4a0058c008a6e8fa0855a69d57b0cb500271c7b48c1a28512608ecd",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.7.2.tar.gz",
```

0.7.1:

```bzl
  strip_prefix = "toolchains_llvm-0.7.1",
  shasum = "5613b430a6b7f6d0eb03011976df53abe7f4cc6c3ec43be066b679c4ad81e3bf",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.7.1.tar.gz",
```

0.7:

```bzl
  strip_prefix = "toolchains_llvm-0.7",
  shasum = "bb07651178c6fbdc0981799b96a09ea5b4f01d98a98ca64c679db1601a92a66f",
  url = "https://github.com/bazel-contrib/toolchains_llvm/archive/refs/tags/0.7.tar.gz",
```

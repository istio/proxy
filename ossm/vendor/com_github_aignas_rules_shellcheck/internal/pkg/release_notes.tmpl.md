## Using Bzlmod with Bazel 6

**NOTE: bzlmod support is still beta. APIs subject to change.**

Add to your `MODULE.bazel` file:

```starlark
bazel_dep(name = "rules_shellcheck", version = "%%TAG%%")
```

## Legacy: using WORKSPACE

Paste this snippet into your `WORKSPACE` file:

```starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_shellcheck",
    sha256 = "%%SHA256%%",
    url = "https://github.com/aignas/rules_shellcheck/releases/download/%%TAG%%/rules_shellcheck-%%TAG%%.tar.gz",
)

load("@rules_shellcheck//:deps.bzl", "shellcheck_dependencies")

shellcheck_dependencies()
```

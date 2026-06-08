---
title: Patching changes to rules_nodejs
layout: default
toc: true
---

# Patching changes

One advantage of open-source software is that you can make your own changes that suit your needs.

The `@rules_nodejs` module can simply be fetched from sources in place of the published distribution.
For example, in place of

```starlark
http_archive(
    name = "rules_nodejs",
    sha256 = "...",
    urls = ["https://github.com/bazel-contrib/rules_nodejs/releases/download/6.0.0/rules_nodejs-6.0.0.tar.gz"],
)
```

you can just use a commit from your fork:

```starlark
http_archive(
    name = "rules_nodejs",
    sha256 = "...",
    strip_prefix = "rules_nodejs-abcd123",
    url = "https://github.com/my-org/rules_nodejs/archive/abcd123.tar.gz",
)
```

## Patching the rules_nodejs release

Bazel has a handy patching mechanism that lets you easily apply a local patch to the release artifact for built-in rules: [the `patches` attribute to `http_archive`](https://docs.bazel.build/versions/master/repo/http.html#attributes).

First, make your changes in a clone of the rules_nodejs repo. Export a patch file simply using `git diff`:

```sh
git diff > my.patch
```

Then copy the patch file somewhere in your repo and point to it from your `WORKSPACE` file:

```python
http_archive(
    name = "rules_nodejs",
    patch_args = ["-p1"],
    patches = ["//path/to/my.patch"],
    sha256 = "...",
    urls = ["https://github.com/bazel-contrib/rules_nodejs/releases/download/6.0.0/rules_nodejs-6.0.0.tar.gz"],
)
```

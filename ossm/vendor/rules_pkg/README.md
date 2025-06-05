# Bazel package building

Bazel rules for building tar, zip, deb, and rpm for packages.

For the latest version, see [Releases](https://github.com/bazelbuild/rules_pkg/releases) (with `WORKSPACE` setup) /
[Documentation](https://bazelbuild.github.io/rules_pkg)

Use rules-pkg-discuss@googlegroups.com for discussion.

CI:
[![Build status](https://badge.buildkite.com/e12f23186aa579f1e20fcb612a22cd799239c3134bc38e1aff.svg)](https://buildkite.com/bazel/rules-pkg)

## Basic rules

### Package building rules

*   [pkg](https://github.com/bazelbuild/rules_pkg/tree/main/pkg) - Rules for
    building packages of various types.
*   [examples](https://github.com/bazelbuild/rules_pkg/tree/main/examples) -
    Cookbook examples for using the rules.

As of Bazel 4.x, Bazel uses this rule set for packaging its distribution. Bazel
still contains a limited version of `pkg_tar` but its feature set is frozen.
Any new capabilities will be added here.


## WORKSPACE setup

Sample, but see [releases](https://github.com/bazelbuild/rules_pkg/releases) for the current release.

```starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "rules_pkg",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
    ],
    sha256 = "8f9ee2dc10c1ae514ee599a8b42ed99fa262b757058f65ad3c384289ff70c4b8",
)
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
rules_pkg_dependencies()
```

To use `pkg_rpm()`, you must provide a copy of `rpmbuild`. You can use the
system installed `rpmbuild` with this stanza.
```starlark
load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")

find_system_rpmbuild(
    name = "rules_pkg_rpmbuild",
    verbose = False,
)
```

## MODULE.bazel setup

```starlark
bazel_dep(name = "rules_pkg", version = "0.0.10")
```
To use `pkg_rpm()`, you must provide a copy of `rpmbuild`. You can use the
system installed `rpmbuild` with this stanza.
```starlark
find_rpm = use_extension("//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild_bzlmod")
use_repo(find_rpm, "rules_pkg_rpmbuild")
register_toolchains("@rules_pkg_rpmbuild//:all")
```

### For developers

*   [Contributor information](CONTRIBUTING.md) (including contributor license agreements)
*   [Patch process](patching.md)
*   [Coding guidelines](developers.md) and other developer information

We hold an engineering status meeting on the first Monday of every month at 10am USA East coast time.
[Add to calendar](https://calendar.google.com/event?action=TEMPLATE&tmeid=MDE2ODMzazlwZnRxbWtkZG5wa2hlYjllMGVfMjAyMTA1MDNUMTUwMDAwWiBjXzUzcHBwZzFudWthZXRmb3E5NzhxaXViNmxzQGc&tmsrc=c_53pppg1nukaetfoq978qiub6ls%40group.calendar.google.com&scp=ALL) /
[meeting notes](https://docs.google.com/document/d/1wkY8ZIcrG8tlKCHzv4st-EltsdlpQENH58fguRnErWY/edit?usp=sharing)

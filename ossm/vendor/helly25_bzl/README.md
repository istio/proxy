# Helly25 bzl, a Bazel support library

This library provides [Bazel](http://bazel.build) [Starlark](https://bazel.build/rules/language) functionality meant to help in maintaining other libraries.

[![Test](https://github.com/helly25/bzl/actions/workflows/main.yml/badge.svg)](https://github.com/helly25/bzl/actions/workflows/main.yml)

## Versions

Implements versioning functions that mostly follow [Semver](https://semver.org/).

Comparators correctly respect major, minor and patch components, as well as
Semver compliant 'pre-release' and 'build' components. The pre-release and build
components are split at ".". Comparing pre-release parts works for alphabetical
prefixes and numeric suffixes, so 'alpha', 'beta' and 'rc' as well as numbered
version of those (e.g. 'alpha1' or rc-1') are supported. For pre-releases and
build pieces a single '-' in front of the numeric parts is dropped (e.g. 'rc-1'
becomes 'rc' + '1' while 'alpha--2' becomes 'alpha-' + '2').

The full functionality is exposed as a singele struct containing all functions.

The version parameters support:
- a string that can be parsed according to:
     `major`['.' `minor` [ '.' `patch` [ '.' `digits`]\*]] ['-' [^+]+] ['+' .\*]
- a `list` or `tuple` where each component is a version part. If present, then:
  - a pre-release component must be separated by a single "-" and split by ".".
  - a build component must be separated by a single "+" and split by "."
- a single `int` which will be the major version.
- anything else is an error and the functions will `fail`.
- unlike Semver, the function allows any number of numeric version components.

Note: Most functions support a `skip_build` parameter. If `True`, then any
present build component will be dropped. Conclusively the parameter is `True`
by default for parsing and `False` for comparisons since Semver dictates that
that the build component must be ignored for precedence (see
[Semver-10](https://semver.org/#spec-item-10)).

The functionality has exhaustive tests. If something still works wrong please,
file a bug report or propose a fix.

Example:
```bazel
my_version = "25.33.42"
min_version = (10, 11, 12)
if _versions.lt(my_version, min_version):
  fail("My version {my_version} is earlier than {min_version}.".format(
    my_version = my_version,
    min_version = min_version,
  ))
```

Provides:

* `load("@helly25_bzl//bzl/versions:versions_bzl", _versions = "versions")`
  * `versions` is a single import structure:
    * `parse`: Parses a version.
    * `ge`: Implements `L >= R`.
    * `gt`: Implements `L > R`.
    * `le`: Implements `L <= R`.
    * `lt`: Implements `L < R`.
    * `eq`: Implements `L == R`.
    * `ne`: Implements `L != R`.
    * `cmp`: Implements `L <=> R` aka `(L < R) - (L > R)`.
    * `compare`: Implements `L OP R`.
    * `check_one_requirement`: Checks a version adheres to a single requirement.
    * `check_all_requirements`: Checks a version adheres to a requirements list.
    * `parse_requirements`: Parses a requirements specification.

## Installation

The library is available for both Bazelmod and Workspace installations and works
on MacOS, Ubuntu and Windows with Bazel version 7.x and 8.x (Other systems are
simply not tested). However future version may drop Windows support.

### For MODULES.bazel

See https://github.com/helly25/bzl/releases to replace the version number.

```
bazel_dep(name = "helly25_bzl", version = "0.0.0")
```

### For WORKSPACE

```bazel
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
  name = "helly25_bzl",
  url = "https://github.com/helly25/bzl/releases/download/0.0.0/bzl-0.0.0.tar.gz",
  sha256 = "...." # see https://github.com/helly25/bzl/releases for version numbers SHA256 codes.
)
```

### Dependencies

* `bazel_skylib`.

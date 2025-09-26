# rules_pkg - Information for Developers

NOTE: This is perpetually a work in progress. We will revise it as we need.

## PR guidelines

Please discuss proposed major features and structural changes before sending a
PR. The best way to do that is to do one or more of the following.

1.  Create an issue
1.  Discuss it in the issue or in rules-pkg-discuss@googlegroups.com
1.  Bring it up at the monthly engineering meeting. (See #325 for
    details)[https://github.com/bazelbuild/rules_pkg/issues/325].

### A few small PRs are better than one big one.

If you need to refactor a lot of code before you can add new behavior, please
send a refactoring PR first (which should not add or change tests), then send a
smaller change that implements the new behavior.

## Functionality

### General solutions are better than package specific ones.

-   Favor solutions that can be reused for all package formats. We will not
    accept contributions that add specific features to one package format when
    they could be incorporated into existing `pkg_files` and related rules such
    that they could be used by all formats.
-   We are moving towards aligning attribute names across rules. If we want
    backwards compatibility with older versions, do that in the macro wrapper.

### Tests

-   All behavioral changes require tests. Even if you are fixing what is most
    likely a bug, please add a test that would have failed before the fix and
    passes after.
-   Aim for minimal tests. Favor unit tests over integration tests. It is better
    to have many small tests rather than one that checks many conditions.
-   Continuous testing is defined under `.bazel_ci`.

### Portability

-   All of the code must work on Linux, macOS, and Windows. Other OSes are
    optional, but we do not have CI to ensure it. If you are making changes that
    require something specifically for portability, your change should include
    inline comments about why, so a future maintainer does not accidentally
    revert it.
-   Some features can not work on all platforms. Favor solutions that allow
    auto-skipping tests that are platform specific rather than requiring
    exclusion lists in CI.

### Major features require commitment

We would love to have features like an MSI builder or macOS application support.
Before accepting a PR for something like that, we want to know that your
organization will commit to maintaining that feature and responding to issues.

## About the code

### Repository structure: run time vs build time

-   pkg/... contains what is needed at run time. Everything else is not part of
    the packaged distribution releases.
-   docs/... is served as github pages. We mostly generate that rather than edit
    by hand.
-   distro/... contains the rules to create the distribution package.
-   examples/... contains runnable examples  which serve as additional
    documentation. These are tested in CI.
-   tests/... contains unit and integration tests. It is a distinct folder so
    that it is not needed in the distribution runtime.
-   <root> contains shims for the .bzl files in pkg/*.bzl. They add backwards
    compatibility to to older releases.

### Starlark style

-   We are moving towards fully generated docs. If you touch an attribute, you
    must update the docstring to explain it.
-   Add docstrings args defined in macros. We are targeting the future time when
    stardoc can build unified documentation for the rule and the wrapper.
-   Actions should not write quoted strings to command lines. If your rule must
    pass file paths to another program, write the paths to an intermediate file
    and pass that as an arg to the other program.
    [See #214](https://github.com/bazelbuild/rules_pkg/issues/214).
-   Run buildifier on you .bzl files before sending a PR.

### Python style

-   We use Python 3. We are not trying to support Python 2 at all.
-   Always import with a full path from the root of the source tree.
-   Compatibility back to python 3.6 (for CentOS 7 and Ubuntu 18.04 LTS) is
    required.

### Dependencies

-   No new dependencies on language toolchains. We take a dependency on Python
    because it is generally available on all OSes.
-   No new python package dependencies.  Use only what is part of the core
    distribution.
-   Other new dependencies are strongly discouraged. The exception is that we
    may take dependencies on other modules maintained by the Bazel team.

### Write for vendoring the source tree.

We presume that some users will vendor this entire rule set into their source
tree and want to test it from their WORKSPACE. Towards that end we try to
minimize the places where we assume the path to any package is absolute
from WORKSPACE. See tests/package_naming_aggregate_test.sh for an example
of how we can write a sh_test that works after re-rooting the sources.

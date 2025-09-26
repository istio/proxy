# Dev Guide

This document covers tips and guidance for working on the `rules_python` code
base. Its primary audience is first-time contributors.

## Running tests

Running tests is particularly easy thanks to Bazel, simply run:

```
bazel test //...
```

And it will run all the tests it can find. The first time you do this, it will
probably take a long time because various dependencies will need to be downloaded
and set up. Subsequent runs will be faster, but there are many tests, and some of
them are slow. If you're working on a particular area of code, you can run just
the tests in those directories instead, which can speed up your edit-run cycle.

## Writing Tests

Most code should have tests of some sort. This helps us have confidence that
refactors didn't break anything and that releases won't have regressions.

We don't require 100% test coverage; testing certain Bazel functionality is
difficult, and some edge cases are simply too hard to test or not worth the
extra complexity. We try to judiciously decide when not having tests is a good
idea.

Tests go under `tests/`. They are loosely organized into directories for the
particular subsystem or functionality they are testing. If an existing directory
doesn't seem like a good match for the functionality being tested, then it's
fine to create a new directory.

Re-usable test helpers and support code go in `tests/support`. Tests don't need
to be perfectly factored and not every common thing a test does needs to be
factored into a more generally reusable piece. Copying and pasting is fine. It's
more important for tests to balance understandability and maintainability.

### Test utilities

General code to support testing is in {gh-path}`tests/support`. It has a variety
of functions, constants, rules etc, to make testing easier. Below are some
common utilities that are frequently used.

### sh_py_run_test

The {gh-path}`sh_py_run_test <tests/support/sh_py_run_test.bzl` rule is a helper to
make it easy to run a Python program with custom build settings using a shell
script to perform setup and verification. This is best to use when verifying
behavior needs certain environment variables or directory structures to
correctly and reliably verify behavior.

When adding a test, you may find the flag you need to set isn't supported by
the rule. To have it support setting a new flag, see the py_reconfig_test docs
below.

### py_reconfig_test

The `py_reconfig_test` and `py_reconfig_binary` rules are helpers for running
Python binaries and tests with custom build flags. This is best to use when
verifying behavior that requires specific flags to be set and when the program
itself can verify the desired state.

They are located in {gh-path}`tests/support/py_reconfig.bzl`

When adding a test, you may find the flag you need to set isn't supported by
the rule. To have it support setting a new flag:

* Add an attribute to the rule. It should have the same name as the flag
  it's for. It should be a string, string_list, or label attribute -- this
  allows distinguishing between if the value was specified or not.
* Modify the transition and add the flag to both the inputs and outputs
  list, then modify the transition's logic to check the attribute and set
  the flag value if the attribute is set.

### whl_from_dir_repo

The `whl_from_dir_repo` repository rule in {gh-path}`tests/support/whl_from_dir`
takes a directory tree and turns it into a `.whl` file. This can be used to
create arbitrary whl files to verify functionality.

### Integration tests

An integration test is one that runs a separate Bazel instance inside the test.
These tests are discouraged unless absolutely necessary because they are slow,
require a lot of memory and CPU, and are generally harder to debug. Integration
tests are reserved for things that simply can't be tested otherwise, or for
simple high-level verification tests.

Integration tests live in `tests/integration`. When possible, add to an existing
integration test.

## Updating internal dependencies

1. Modify the `./python/private/pypi/requirements.txt` file and run:
   ```
   bazel run //private:whl_library_requirements.update
   ```
1. Run the following target to update `twine` dependencies:
   ```
   bazel run //private:requirements.update
   ```
1. Bump the coverage dependencies using the script using:
   ```
   bazel run //tools/private/update_deps:update_coverage_deps <VERSION>
   # for example:
   # bazel run //tools/private/update_deps:update_coverage_deps 7.6.1
   ```

## Updating tool dependencies

It's suggested to routinely update the tool versions within our repo. Some of the
tools are using requirement files compiled by `uv`, and others use other means. In order
to have everything self-documented, we have a special target,
`//private:requirements.update`, which uses `rules_multirun` to run all
of the requirement-updating scripts in sequence in one go. This can be done once per release as
we prepare for releases.

## Creating Backport PRs

The steps to create a backport PR are:

1.  Create an issue for the patch release; use the [patch release
    template][patch-release-issue].
2.  Create a fork of `rules_python`.
3.  Checkout the `release/X.Y` branch.
4.  Use `git cherry-pick -x` to cherry pick the desired fixes.
5.  Update the release's `CHANGELOG.md` file:
    * Add a Major.Minor.Patch section if one doesn't exist
    * Copy the changelog text from `main` to the release's changelog.
6.  Send a PR with the backport's changes.
    * The title should be `backport: PR#N to Major.Minor`
    * The body must preserve the original PR's number, commit hash, description,
      and authorship.
      Use the following format (`git cherry-pick` will use this format):
      ```
      <original PR title>

      <original PR body>
      (cherry picked from commit <commit hash>)
      -----
      Co-authored-by: <original PR author; separate lines for each>
      ```
    * If the PR contains multiple backport commits, separate each's description
      with `-----`.
7.  Send a PR to update the `main` branch's `CHANGELOG.md` to reflect the
    changes done in the patched release.

[patch-release-issue]: https://github.com/bazelbuild/rules_python/issues/new?template=patch_release.md
